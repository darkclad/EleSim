#include "UndoManager.h"

UndoManager::UndoManager(QObject* parent)
    : QObject(parent)
{
}

void UndoManager::pushState(const QJsonObject& state)
{
    m_undoStack.push_back(state);
    if (m_undoStack.size() > MAX_STACK_SIZE)
        m_undoStack.removeFirst();
    m_redoStack.clear();
    emit stateChanged(canUndo(), canRedo());
}

QJsonObject UndoManager::undo(const QJsonObject& currentState)
{
    if (m_undoStack.isEmpty()) return {};
    m_redoStack.push_back(currentState);
    QJsonObject previous = m_undoStack.takeLast();
    emit stateChanged(canUndo(), canRedo());
    return previous;
}

QJsonObject UndoManager::redo(const QJsonObject& currentState)
{
    if (m_redoStack.isEmpty()) return {};
    m_undoStack.push_back(currentState);
    QJsonObject next = m_redoStack.takeLast();
    emit stateChanged(canUndo(), canRedo());
    return next;
}

void UndoManager::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit stateChanged(canUndo(), canRedo());
}
