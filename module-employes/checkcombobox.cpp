#include "checkcombobox.h"
#include <QStandardItemModel>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QMouseEvent>

CheckComboBox::CheckComboBox(QWidget *parent) : QComboBox(parent)
{
    m_model = new QStandardItemModel(this);
    setModel(m_model);
    setEditable(true);
    lineEdit()->setReadOnly(true);
    lineEdit()->setPlaceholderText("Selectionner une ou plusieurs equipes");
    view()->viewport()->installEventFilter(this);
    lineEdit()->installEventFilter(this);
    connect(m_model, &QStandardItemModel::itemChanged, this, [this]() { updateText(); });
}

void CheckComboBox::addItem_(const QString &t)
{
    auto *it = new QStandardItem(t);
    it->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    it->setData(Qt::Unchecked, Qt::CheckStateRole);
    m_model->appendRow(it);
}

void CheckComboBox::setItems(const QStringList &items)
{
    m_model->clear();
    for (const QString &t : items) addItem_(t);
    updateText();
}

QStringList CheckComboBox::checkedItems() const
{
    QStringList out;
    for (int i = 0; i < m_model->rowCount(); ++i)
        if (m_model->item(i)->checkState() == Qt::Checked) out << m_model->item(i)->text();
    return out;
}

void CheckComboBox::setCheckedItems(const QStringList &items)
{
    m_model->blockSignals(true);
    for (int i = 0; i < m_model->rowCount(); ++i) m_model->item(i)->setCheckState(Qt::Unchecked);
    for (const QString &t : items) {
        bool found = false;
        for (int i = 0; i < m_model->rowCount(); ++i)
            if (m_model->item(i)->text().compare(t, Qt::CaseInsensitive) == 0) {
                m_model->item(i)->setCheckState(Qt::Checked); found = true; break;
            }
        if (!found) { addItem_(t); m_model->item(m_model->rowCount() - 1)->setCheckState(Qt::Checked); }
    }
    m_model->blockSignals(false);
    updateText();
}

void CheckComboBox::clearChecked() { setCheckedItems(QStringList()); }

void CheckComboBox::updateText()
{
    lineEdit()->setText(checkedItems().join(", "));
}

bool CheckComboBox::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == view()->viewport() && ev->type() == QEvent::MouseButtonRelease) {
        QModelIndex idx = view()->indexAt(static_cast<QMouseEvent *>(ev)->pos());
        if (idx.isValid()) {
            auto *it = m_model->itemFromIndex(idx);
            it->setCheckState(it->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            return true;
        }
    }
    if (obj == lineEdit() && ev->type() == QEvent::MouseButtonRelease) {
        showPopup();
        return true;
    }
    return QComboBox::eventFilter(obj, ev);
}