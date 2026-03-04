#pragma once

#include "MNASolver.h"
#include "TransientState.h"
#include <mutex>
#include <vector>
#include <QString>

class SharedFrameBuffer {
public:
    // Called by worker thread: append a batch of new frames
    void appendFrames(std::vector<TransientFrame>&& frames) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingFrames.insert(m_pendingFrames.end(),
                               std::make_move_iterator(frames.begin()),
                               std::make_move_iterator(frames.end()));
    }

    // Called by worker thread: mark simulation as complete, provide final state
    void markComplete(bool success, const QString& error, TransientState&& finalState) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_success = success;
        m_error = error;
        m_finalState = std::move(finalState);
        m_complete = true;
    }

    // Called by UI thread: drain pending frames into the destination vector
    size_t drainFrames(std::vector<TransientFrame>& dest) {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = m_pendingFrames.size();
        if (count > 0) {
            dest.insert(dest.end(),
                        std::make_move_iterator(m_pendingFrames.begin()),
                        std::make_move_iterator(m_pendingFrames.end()));
            m_pendingFrames.clear();
        }
        return count;
    }

    bool isComplete() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_complete;
    }

    bool wasSuccessful() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_success;
    }

    QString errorMessage() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_error;
    }

    TransientState takeFinalState() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::move(m_finalState);
    }

private:
    mutable std::mutex m_mutex;
    std::vector<TransientFrame> m_pendingFrames;
    bool m_complete = false;
    bool m_success = false;
    QString m_error;
    TransientState m_finalState;
};
