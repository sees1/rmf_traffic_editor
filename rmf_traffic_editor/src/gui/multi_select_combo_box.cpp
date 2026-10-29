#include <multi_select_combo_box.h>

#include <iostream>

void MultiSelectComboBox::buildList()
{
  p_list_widget = new QListWidget(this);
  p_line_edit = new QLineEdit(this);
  p_line_edit->setReadOnly(true);

  connect(
    p_line_edit,
    &QLineEdit::textChanged,
    [this](const QString& text)
    {
      textChanged(text);
    }
  );

  for (std::size_t i = 0; i < selections.size(); i++)
  {
    QListWidgetItem* pListItem = new QListWidgetItem(p_list_widget);
    p_list_widget->addItem(pListItem);
    QCheckBox* pCheckBox =
      new QCheckBox(QString::fromStdString(selections[i].first));
    p_list_widget->setItemWidget(pListItem, pCheckBox);

    connect(
      pCheckBox,
      &QCheckBox::stateChanged,
      [this](int state)
      {
        boxChecked(state);
      }
    );
  }

  this->setModel(p_list_widget->model());
  this->setView(p_list_widget);
  this->setLineEdit(p_line_edit);
}


void MultiSelectComboBox::boxChecked(int /*state*/)
{
  blockSignals(true);
  std::size_t list_count = p_list_widget->count();
  selected_text.clear();

  for (std::size_t i = 0; i < list_count; i++)
  {
    QListWidgetItem* pItem = p_list_widget->item(i);
    QCheckBox* pCheckBox = static_cast<QCheckBox*>(p_list_widget->itemWidget(
        pItem));
    if (pCheckBox->isChecked())
    {
      QString checkbox_text = pCheckBox->text();
      selected_text.append(checkbox_text).append(";");
      selections[i].second = true;
    }
    else
    {
      selections[i].second = false;
    }
  }

  if (selected_text.isEmpty())
  {
    p_line_edit->clear();
    return;
  }
  p_line_edit->setText(selected_text);
  p_line_edit->setToolTip(selected_text);
  blockSignals(false);
}

void MultiSelectComboBox::textChanged(const QString& text)
{
  p_line_edit->setText(text);
}

std::vector<std::string> MultiSelectComboBox::getCheckResult()
{
  std::vector<std::string> result;
  for (auto item : selections)
  {
    if (item.second)
    {
      result.emplace_back(item.first);
    }
  }
  return result;
}
