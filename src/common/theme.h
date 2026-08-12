#pragma once

#include <QApplication>

enum class ColorScheme { Dark, Light, Unknown };

ColorScheme detectColorScheme(QApplication *app);