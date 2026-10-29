/*
 * Copyright (C) 2019 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <string>

#include <QSettings>
#include <QtWidgets>

#include "glog/logging.h"

#include "editor.h"
#include "preferences_keys.h"


int main(int argc, char* argv[])
{
  google::InitGoogleLogging(argv[0]);  // used later by Ceres

  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
  QApplication app(argc, argv);
  app.setStyle(QStyleFactory::create("Fusion"));
  QPalette palette = app.style()->standardPalette();
  palette.setColor(QPalette::Window, QColor("#f4f6fa"));
  palette.setColor(QPalette::Base, Qt::white);
  palette.setColor(QPalette::Text, QColor("#1e293b"));
  palette.setColor(QPalette::WindowText, QColor("#1e293b"));
  palette.setColor(QPalette::Highlight, QColor("#2563eb"));
  palette.setColor(QPalette::HighlightedText, Qt::white);
  app.setPalette(palette);
  QFile stylesheet(":/styles/light.qss");
  if (stylesheet.open(QIODevice::ReadOnly))
    app.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
  app.setOrganizationName("open-robotics");
  app.setOrganizationDomain("openrobotics.org");
  app.setApplicationName("traffic-editor");

  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addPositionalArgument("[building]", "Building JSON or YAML file to open");
  parser.process(QCoreApplication::arguments());

  Editor editor;
  QSettings settings;

  const bool load_previous = settings.value(
    preferences_keys::open_previous_building, QVariant(true)).toBool();

  if (load_previous && parser.positionalArguments().isEmpty())
    editor.loadPreviousBuilding();

  if (parser.positionalArguments().length() >= 1)
    editor.loadBuilding(parser.positionalArguments().at(0));

  editor.show();

  // Current understanding is that the viewport needs to be restored
  // after the editor.show() is called
  editor.restorePreviousViewport();

  return app.exec();
}
