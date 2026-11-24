#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <utility>

// Fila thread-safe simples.
// - Um produtor e um consumidor por fila (o caso do projeto).
// - push() nao bloqueia; pop() bloqueia ate ter item ou ate shutdown().

template <typename T>
class Queue {
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lk(m_);
            q_.push_back(std::move(value));
        }
        cv_.notify_one();
    }

    // Tenta retirar um item sem bloquear.
    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lk(m_);
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }

    // Bloqueia ate ter item ou a fila ser desligada.
    // Retorna false se foi desligada e nao ha itens.
    bool pop(T& out) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return shutdown_ || !q_.empty(); });
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(m_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(m_);
        return q_.empty();
    }

private:
    mutable std::mutex      m_;
    std::condition_variable cv_;
    std::deque<T>           q_;
    bool                    shutdown_ = false;
};