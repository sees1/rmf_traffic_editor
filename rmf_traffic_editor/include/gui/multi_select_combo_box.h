#ifndef MULTI_SELECT_COMBO_BOX__H
#define MULTI_SELECT_COMBO_BOX__H

#include <QComboBox>
#include <QtWidgets>

#include <string>
#include <sstream>
#include <vector>
#include <set>

class MultiSelectComboBox : public QComboBox
{
public:
  template<typename ITEM_TYPE>
  MultiSelectComboBox(const std::vector<ITEM_TYPE>& selection_list)
  {
    selections.clear();
    for (auto item : selection_list)
    {
      selections.emplace_back(typeToString(item), false);
    }
    buildList();
  }

  ~MultiSelectComboBox() {}

  template<typename ITEM_TYPE>
  void showCheckedItem(const std::set<ITEM_TYPE>& checked_list)
  {
    std::set<std::string> checked_item;
    for (auto item : checked_list)
    {
      checked_item.insert(typeToString(item) );
    }

    auto list_count = p_list_widget->count();
    for (auto i = 0; i < list_count; i++)
    {
      QListWidgetItem* pItem = p_list_widget->item(i);
      QCheckBox* pCheckBox =
        static_cast<QCheckBox*>(p_list_widget->itemWidget(pItem));
      auto checkbox_text = pCheckBox->text();
      if (checked_item.find(checkbox_text.toStdString() ) ==
        checked_item.end() )
      {
        // original goal area in project might be deleted, so we won't update the old version
        continue;
      }
      pCheckBox->setCheckState(Qt::Checked);
    }
  }

  template<typename ITEM_TYPE>
  std::string typeToString(ITEM_TYPE& input)
  {
    std::stringstream s;
    std::string temp;
    s << input;
    s >> temp;
    return temp;
  }

  std::vector<std::string> getCheckResult();

private:
  void buildList();

  void boxChecked(int state);

  void textChanged(const QString& text);

private:
  std::vector<std::pair<std::string, bool>> selections;
  QString selected_text;
  QListWidget* p_list_widget;
  QLineEdit* p_line_edit;
};

#endif