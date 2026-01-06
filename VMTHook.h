#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring> // Required for memcpy

class VMTHook {
public:
    VMTHook() : m_pObject(nullptr), m_pOriginalVTable(nullptr), m_pNewVTable(nullptr), m_nVTableSize(0) {}

    ~VMTHook() {
        Restore();
        if (m_pNewVTable) delete[] m_pNewVTable;
    }

    bool Init(void* pObject) {
        if (!pObject) return false;

        // [FIX] Clean up previous hook if re-initializing
        if (m_pNewVTable) {
            delete[] m_pNewVTable;
            m_pNewVTable = nullptr;
        }

        m_pObject = (void***)pObject;
        m_pOriginalVTable = *m_pObject;

        // [FIX] Reset size counter for the new object
        m_nVTableSize = 0;

        // Count VTable size
        while (reinterpret_cast<uintptr_t*>(m_pOriginalVTable)[m_nVTableSize]) {
            m_nVTableSize++;
            if (m_nVTableSize > 1000) break; // Safety break
        }

        // Copy VTable
        m_pNewVTable = new void* [m_nVTableSize];
        memcpy(m_pNewVTable, m_pOriginalVTable, m_nVTableSize * sizeof(void*));

        // Apply Swap
        *m_pObject = m_pNewVTable;
        return true;
    }

    void Restore() {
        // Only restore if we are currently hooked
        if (m_pObject && m_pOriginalVTable && *m_pObject == m_pNewVTable) {
            *m_pObject = m_pOriginalVTable;
        }
    }

    template<typename T>
    T Hook(int index, void* pNewFunction) {
        if (index < 0 || index >= m_nVTableSize || !m_pNewVTable) return nullptr;
        m_pNewVTable[index] = pNewFunction;
        return reinterpret_cast<T>(m_pOriginalVTable[index]);
    }

    template<typename T>
    T GetOriginal(int index) {
        if (!m_pOriginalVTable || index < 0 || index >= m_nVTableSize) return nullptr;
        return reinterpret_cast<T>(m_pOriginalVTable[index]);
    }

private:
    void*** m_pObject;
    void** m_pOriginalVTable;
    void** m_pNewVTable;
    int m_nVTableSize;
};