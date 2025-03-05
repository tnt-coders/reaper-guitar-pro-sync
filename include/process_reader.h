#pragma once

#include <windows.h> // Must be included before tlhelp32.h
#include <tlhelp32.h>

#include <codecvt>
#include <format>
#include <memory>
#include <stdexcept>
#include <vector>

namespace tnt {

class ProcessReader final
{
public:
    ProcessReader(const std::wstring& process_name, const std::wstring& module_name)
        : m_process_name(process_name)
        , m_module_name(module_name)
    {
        m_process_id = this->GetProcessID(m_process_name.c_str());
        if (!m_process_id)
        {
            throw std::runtime_error(std::format("Failed to get process ID for process '{}'.", WStringToString(m_process_name)));
        }

        m_module_base_address = this->GetModuleBaseAddress(m_module_name.c_str());
        if (!m_module_base_address)
        {
            throw std::runtime_error(std::format("Failed to get module base address for module '{}'.", WStringToString(m_module_name)));
        }

        m_process_handle = OpenProcess(PROCESS_VM_READ, FALSE, m_process_id);
        if (!m_process_handle) {
            throw std::runtime_error(std::format("Failed to open process '{}'.", WStringToString(m_process_name)));
        }
    }

    ~ProcessReader()
    {
        CloseHandle(m_process_handle);
    }

    template <typename T>
    T ReadMemoryAddress(const DWORD_PTR module_offset, const std::vector<DWORD_PTR> pointer_offsets)
    {
        DWORD_PTR base_address = m_module_base_address + module_offset;
        DWORD_PTR address = this->ReadPointer(base_address, pointer_offsets);

        // Value stored at the memory address
        T value;

        // Attempt to read memory
        if (!ReadProcessMemory(m_process_handle, reinterpret_cast<LPCVOID>(address), &value, sizeof(value), nullptr))
        {
            // If ReadProcessMemory fails, get the last error code
            DWORD error = GetLastError();
            std::string error_message = [&] {
                switch(error)
                {
                case ERROR_ACCESS_DENIED:
                    return "Access denied. Make sure the process is accessible.";
                case ERROR_INVALID_PARAMETER:
                    return "Invalid parameter passed to ReadProcessMemory.";
                case ERROR_PARTIAL_COPY:
                    return "Partial copy, the memory range is inaccessible.";
                default:
                    return "Unknown error.";
                }
            }();

            throw std::runtime_error(std::format("Failed to read memory at address {} for process '{}': {}", address, WStringToString(m_process_name), error_message));
        }

        return value;
    }

private:
    std::wstring m_process_name;
    std::wstring m_module_name;
    DWORD m_process_id;
    DWORD_PTR m_module_base_address;
    HANDLE m_process_handle;
    
    DWORD GetProcessID(const wchar_t* process_name)
    {
        HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot_handle == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        PROCESSENTRY32 process_entry;
        process_entry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot_handle, &process_entry))
        {
            do
            {
                if (_wcsicmp(process_entry.szExeFile, process_name) == 0)
                {
                    CloseHandle(snapshot_handle);
                    return process_entry.th32ProcessID;
                }
            } while (Process32Next(snapshot_handle, &process_entry));
        }

        CloseHandle(snapshot_handle);

        return 0;
    }

    DWORD_PTR GetModuleBaseAddress(const wchar_t* module_name)
    {
        const HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_process_id);
        if (snapshot_handle == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        MODULEENTRY32W module_entry;
        module_entry.dwSize = sizeof(module_entry);

        if (Module32FirstW(snapshot_handle, &module_entry))
        {
            do
            {
                if (_wcsicmp(module_entry.szModule, module_name) == 0)
                {
                    CloseHandle(snapshot_handle);
                    return reinterpret_cast<DWORD_PTR>(module_entry.modBaseAddr);
                }
            } while (Module32NextW(snapshot_handle, &module_entry));
        }

        CloseHandle(snapshot_handle);

        return 0;
    }

    DWORD_PTR ReadPointer(const DWORD_PTR base_address, const std::vector<DWORD_PTR> offsets)
    {
        DWORD_PTR address = base_address;
        DWORD_PTR temp_address;

        for (size_t i = 0; i < offsets.size(); i++)
        {
            if (!ReadProcessMemory(m_process_handle, (LPCVOID)address, &temp_address, sizeof(temp_address), nullptr))
            {
                return 0;
            }

            address = temp_address + offsets[i];
        }

        return address;
    }

    // Utility function to convert a std::wstring into a std::string
    std::string WStringToString(const std::wstring& wstr)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wstr);
    }
};

}