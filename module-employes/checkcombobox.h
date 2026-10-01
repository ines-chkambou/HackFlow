#ifndef CHECKCOMBOBOX_H
#define CHECKCOMBOBOX_H

#include <QComboBox>
#include <QStringList>
class QStandardItemModel;

class CheckComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit CheckComboBox(QWidget *parent = nullptr);
    void        setItems(const QStringList &items);
    QStringList checkedItems() const;
    void        setCheckedItems(const QStringList &items);
    void        clearChecked();
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
private:
    QStandardItemModel *m_model;
    void updateText();
    void addItem_(const QString &t);
};

#endif // CHECKCOMBOBOX_H