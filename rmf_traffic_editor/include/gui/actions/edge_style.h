#ifndef TRAFFIC_EDITOR_EDGE_STYLE_H
#define TRAFFIC_EDITOR_EDGE_STYLE_H

#include <QUndoCommand>
#include "building.h"

// Store only display parameters, leaving route and simulation data untouched.
class EdgeStyleCommand : public QUndoCommand
{
public:
  EdgeStyleCommand(Building& building, int level,
    const std::vector<int>& indices, const QColor& color, double width)
  : _building(building), _level(level), _indices(indices),
    _color(color.name().toStdString()), _width(width)
  {
    setText("Change line style");
    for (int index : indices)
      _before.push_back(building.levels[level].edges[index].params);
  }

  void redo() override
  {
    for (int index : _indices)
    {
      auto& params = _building.levels[_level].edges[index].params;
      params["editor_color"] = Param(_color);
      params["editor_width"] = Param(_width);
    }
  }

  void undo() override
  {
    for (std::size_t i = 0; i < _indices.size(); ++i)
    {
      auto& params = _building.levels[_level].edges[_indices[i]].params;
      for (const auto* key : {"editor_color", "editor_width"})
      {
        const auto it = _before[i].find(key);
        if (it == _before[i].end())
          params.erase(key);
        else
          params[key] = it->second;
      }
    }
  }

private:
  Building& _building;
  int _level;
  std::vector<int> _indices;
  std::vector<std::map<std::string, Param>> _before;
  std::string _color;
  double _width;
};

#endif
