#pragma once

#include <deque>
#include <mutex>

namespace anxiety::jobs {
    // WorkStealingDeque<T> -----------------------------------------------------------------------
    // Cola de trabajo de doble extremo para el planificador work-stealing.
    //
    //   push_back / pop_back  — usados por el worker dueño (LIFO, favorable a la caché).
    //   steal                 — usado por los ladrones para tomar del principio (FIFO, el trabajo
    //                           más antiguo primero, para maximizar el aprovechamiento).
    //
    // La interfaz (devuelve bool + parámetro de salida) está diseñada para permitir en el futuro un
    // Chase-Lev sin locks sin cambiar ningún punto de llamada.
    // --------------------------------------------------------------------------------------------
    template<typename T>
    class WorkStealingDeque {
    public:
        // El dueño empuja trabajo nuevo por el final.
        void push_back(T item) {
            std::lock_guard lock(m_mu);
            m_items.push_back(std::move(item));
        }

        // El dueño extrae su elemento más reciente por el final (LIFO).
        bool pop_back(T& out) {
            std::lock_guard lock(m_mu);
            if (m_items.empty()) return false;
            out = std::move(m_items.back());
            m_items.pop_back();
            return true;
        }

        // Un ladrón roba el elemento más antiguo por el principio (FIFO).
        bool steal(T& out) {
            std::lock_guard lock(m_mu);
            if (m_items.empty()) return false;
            out = std::move(m_items.front());
            m_items.pop_front();
            return true;
        }

        bool empty() const {
            std::lock_guard lock(m_mu);
            return m_items.empty();
        }

    private:
        std::deque<T>      m_items;
        mutable std::mutex m_mu;
    };
} // namespace anxiety::jobs
