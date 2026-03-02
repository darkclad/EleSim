#include "ComponentToolbar.h"

#include <QToolButton>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QLabel>
#include <QIcon>
#include <QVBoxLayout>
#include <QScrollArea>

// Custom button that initiates drag
class DragButton : public QToolButton
{
public:
    DragButton(ComponentType type, QWidget* parent = nullptr)
        : QToolButton(parent), m_type(type) {}

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragStart = event->pos();
        }
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!(event->buttons() & Qt::LeftButton))
            return;

        if ((event->pos() - m_dragStart).manhattanLength() < 10)
            return;

        auto* mimeData = new QMimeData;
        mimeData->setData("application/x-elesim-component",
                          QByteArray::number(static_cast<int>(m_type)));

        auto* drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->setPixmap(icon().pixmap(32, 32));
        drag->exec(Qt::CopyAction);
    }

private:
    ComponentType m_type;
    QPoint m_dragStart;
};

// Collapsible section: clickable header + hideable content area
class CollapsibleSection : public QWidget
{
public:
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr)
        : QWidget(parent), m_expanded(true)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // Header button
        m_header = new QToolButton(this);
        m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_header->setAutoRaise(true);
        m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_header->setStyleSheet(
            "QToolButton { text-align: left; padding: 4px 6px; border: none; }"
            "QToolButton:hover { background: palette(midlight); }"
        );
        m_title = title;
        updateHeaderText();

        QFont f = m_header->font();
        f.setBold(true);
        m_header->setFont(f);

        layout->addWidget(m_header);

        // Content area
        m_content = new QWidget(this);
        m_contentLayout = new QVBoxLayout(m_content);
        m_contentLayout->setContentsMargins(4, 0, 0, 2);
        m_contentLayout->setSpacing(1);
        layout->addWidget(m_content);

        connect(m_header, &QToolButton::clicked, this, &CollapsibleSection::toggle);
    }

    QVBoxLayout* contentLayout() const { return m_contentLayout; }

    void toggle()
    {
        m_expanded = !m_expanded;
        m_content->setVisible(m_expanded);
        updateHeaderText();
    }

private:
    void updateHeaderText()
    {
        QString arrow = m_expanded ? QStringLiteral("\u25BE ") : QStringLiteral("\u25B8 ");
        m_header->setText(arrow + m_title);
    }

    QToolButton* m_header;
    QWidget* m_content;
    QVBoxLayout* m_contentLayout;
    QString m_title;
    bool m_expanded;
};

ComponentToolbar::ComponentToolbar(QWidget* parent)
    : QToolBar(tr("Components"), parent)
{
    setOrientation(Qt::Vertical);
    setMovable(false);
    setIconSize(QSize(36, 36));

    // Main container
    auto* container = new QWidget(this);
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(2, 2, 2, 2);
    containerLayout->setSpacing(2);

    // --- Sources ---
    auto* sources = addSection(containerLayout, tr("Sources"));
    addButtonToSection(sources, ":/icons/dc_source.svg", "DC Source", "DC Voltage Source\nDrag to place", ComponentType::DCSource);
    addButtonToSection(sources, ":/icons/ac_source.svg", "AC Source", "AC Voltage Source\nDrag to place", ComponentType::ACSource);
    addButtonToSection(sources, ":/icons/dc_current_source.svg", "I Source", "DC Current Source\nDrag to place", ComponentType::DCCurrentSource);
    addButtonToSection(sources, ":/icons/pulse_source.svg", "Pulse", "Pulse Source\nDrag to place", ComponentType::PulseSource);

    // --- Passives ---
    auto* passives = addSection(containerLayout, tr("Passives"));
    addButtonToSection(passives, ":/icons/resistor.svg", "Resistor", "Resistor\nDrag to place", ComponentType::Resistor);
    addButtonToSection(passives, ":/icons/capacitor.svg", "Capacitor", "Capacitor\nDrag to place", ComponentType::Capacitor);
    addButtonToSection(passives, ":/icons/inductor.svg", "Inductor", "Inductor\nDrag to place", ComponentType::Inductor);
    addButtonToSection(passives, ":/icons/diode.svg", "Diode", "Diode\nDrag to place", ComponentType::Diode);
    addButtonToSection(passives, ":/icons/zener_diode.svg", "Zener", "Zener Diode\nDrag to place", ComponentType::ZenerDiode);

    // --- Loads ---
    auto* loads = addSection(containerLayout, tr("Loads"));
    addButtonToSection(loads, ":/icons/lamp.svg", "Lamp", "Lamp\nDrag to place", ComponentType::Lamp);

    // --- Switches ---
    auto* switches = addSection(containerLayout, tr("Switches"));
    addButtonToSection(switches, ":/icons/switch2.svg", "Switch", "Two-way Switch (SPST)\nClick to place", ComponentType::Switch2Way);
    addButtonToSection(switches, ":/icons/switch3.svg", "3-Way", "Three-way Switch (SPDT)\nClick to place", ComponentType::Switch3Way);
    addButtonToSection(switches, ":/icons/switch4.svg", "4-Way", "Four-way Switch (DPDT)\nClick to place", ComponentType::Switch4Way);

    // --- Reference ---
    auto* reference = addSection(containerLayout, tr("Reference"));
    addButtonToSection(reference, ":/icons/ground.svg", "Ground", "Ground Reference\nDrag to place", ComponentType::Ground);

    containerLayout->addStretch();

    // Wrap in scroll area
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);

    addWidget(scrollArea);
}

CollapsibleSection* ComponentToolbar::addSection(QLayout* parentLayout, const QString& title)
{
    auto* section = new CollapsibleSection(title, this);
    parentLayout->addWidget(section);
    return section;
}

void ComponentToolbar::addButtonToSection(CollapsibleSection* section,
                                           const QString& iconPath, const QString& text,
                                           const QString& tooltip, ComponentType type)
{
    auto* btn = new DragButton(type, section);
    btn->setIcon(QIcon(iconPath));
    btn->setText(text);
    btn->setToolTip(tooltip);
    btn->setMinimumSize(72, 52);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    QFont f = btn->font();
    f.setPixelSize(10);
    btn->setFont(f);

    connect(btn, &QToolButton::clicked, this, [this, type]() {
        emit componentSelected(type);
    });

    section->contentLayout()->addWidget(btn);
}
