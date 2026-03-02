#pragma once

#include <QObject>
#include <QJsonObject>
#include <QVector>

class UndoManager : public QObject
{
    Q_OBJECT

public:
    explicit UndoManager(QObject* parent = nullptr);

    void pushState(const QJsonObject& state);

    QJsonObject undo(const QJsonObject& currentState);
    QJsonObject redo(const QJsonObject& currentState);

    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }

    void clear();

signals:
    void stateChanged(bool canUndo, bool canRedo);

private:
    QVector<QJsonObject> m_undoStack;
    QVector<QJsonObject> m_redoStack;
    static constexpr int MAX_STACK_SIZE = 50;
};
