#ifndef _DELETE_H_
#define _DELETE_H_

#include <QUndoCommand>
#include "building.h"

class DeleteCommand : public QUndoCommand
{
public:
  DeleteCommand(Building* building, int level_idx, bool delete_used_entities = false);

  virtual ~DeleteCommand();

  void undo() override;

  void redo() override;

private:
  std::vector<Level> _before;
  std::vector<Level> _after;
  std::map<std::string, std::map<std::string, NavigationVertices>> navigation_before_;
  std::map<std::string, std::map<std::string, NavigationVertices>> navigation_after_;
  bool _applied = false;
  bool delete_used_entities_;
  Building* _building;
  int _level_idx;
};

#endif
