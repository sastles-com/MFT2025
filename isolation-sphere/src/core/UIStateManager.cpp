#include <cmath>
#include "core/UIStateManager.h"

UIStateManager::UIStateManager() {
    reset();
}

void UIStateManager::reset() {
    mode = UIMode::Live;
    functionCount = 8;
    selectedFunctionIndex = 0;
    // 仮の機能名・タイプを初期化
    for (int i = 0; i < 8; ++i) {
        functions[i].name = "func" + std::to_string(i);
        functions[i].type = UIFunctionType::Analog;
        functions[i].index = i;
        functions[i].analogValue = 0.0f;
        functions[i].discreteIndex = 0;
        functions[i].boolValue = false;
    }
}

UIMode UIStateManager::getMode() const {
    return mode;
}

void UIStateManager::nextMode() {
    int m = static_cast<int>(mode);
    m = (m + 1) % 5;
    mode = static_cast<UIMode>(m);
}

void UIStateManager::setMode(UIMode m) {
    mode = m;
}

int UIStateManager::getFunctionCount() const {
    return functionCount;
}

int UIStateManager::getSelectedFunctionIndex() const {
    return selectedFunctionIndex;
}

void UIStateManager::selectFunction(int index) {
    if (index >= 0 && index < functionCount) {
        selectedFunctionIndex = index;
    }
}

const UIFunctionState& UIStateManager::getSelectedFunction() const {
    return functions[selectedFunctionIndex];
}

void UIStateManager::updateAnalogValue(float value) {
    functions[selectedFunctionIndex].analogValue = value;
}

void UIStateManager::updateDiscreteValue(int idx) {
    functions[selectedFunctionIndex].discreteIndex = idx;
}

void UIStateManager::updateBooleanValue(bool v) {
    functions[selectedFunctionIndex].boolValue = v;
}

void UIStateManager::confirmSelection() {
    // 設定確定時の処理（今はダミー）
}

void UIStateManager::onLcdButtonPress() {
    nextMode();
}

void UIStateManager::onLeftStick(float x, float y) {
    // 角度から機能インデックスを決定（仮実装）
    float angle = atan2f(y, x);
    if (angle < 0) angle += 2 * 3.1415926f;
    int idx = static_cast<int>(angle / (2 * 3.1415926f / functionCount));
    selectFunction(idx);
}

void UIStateManager::onRightStick(float x, float y) {
    // 仮：右スティックのx値をアナログ値に反映
    updateAnalogValue(x);
}

void UIStateManager::onStickPress() {
    confirmSelection();
}
