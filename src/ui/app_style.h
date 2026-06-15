#ifndef APP_STYLE_H
#define APP_STYLE_H

#include <QString>

class QPushButton;

namespace UiStyle {

QString globalStyle();

void applyPrimaryButton(QPushButton* button, int min_width = 88);
void applySecondaryButton(QPushButton* button, int min_width = 88);
void applyDangerButton(QPushButton* button, int min_width = 88);
void applyGhostButton(QPushButton* button, int min_width = 88);
void applyDangerGhostButton(QPushButton* button, int min_width = 88);

} // namespace UiStyle

#endif // APP_STYLE_H
