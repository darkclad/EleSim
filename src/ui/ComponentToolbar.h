#pragma once

#include <QToolBar>
#include "core/Component.h"

class QToolButton;
class CollapsibleSection;

class ComponentToolbar : public QToolBar
{
    Q_OBJECT

public:
    explicit ComponentToolbar(QWidget* parent = nullptr);

signals:
    void componentSelected(ComponentType type);

private:
    CollapsibleSection* addSection(QLayout* parentLayout, const QString& title);
    void addButtonToSection(CollapsibleSection* section,
                            const QString& iconPath, const QString& text,
                            const QString& tooltip, ComponentType type);
};
