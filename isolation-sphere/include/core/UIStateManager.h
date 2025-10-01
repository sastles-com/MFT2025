#pragma once
#include <stdint.h>
#include <string>

// UIモード定義
enum class UIMode {
    Live,
    Control,
    Video,
    Maintenance,
    System,
    Unknown
};

// 機能タイプ
enum class UIFunctionType {
    Analog,
    Discrete,
    Boolean
};

struct UIFunctionState {
    std::string name;
    UIFunctionType type;
    int index;
    float analogValue;
    int discreteIndex;
    bool boolValue;
};

class UIStateManager {
public:
    UIStateManager();
    void reset();

    UIMode getMode() const;
    void nextMode();
    void setMode(UIMode mode);

    int getFunctionCount() const;
    int getSelectedFunctionIndex() const;
    void selectFunction(int index);
    const UIFunctionState& getSelectedFunction() const;

    void updateAnalogValue(float value);
    void updateDiscreteValue(int index);
    void updateBooleanValue(bool value);

    void confirmSelection();

    // 物理入力イベントを受け取るAPI例
    void onLcdButtonPress();
    void onLeftStick(float x, float y);
    void onRightStick(float x, float y);
    void onStickPress();

private:
    UIMode mode;
    int functionCount;
    int selectedFunctionIndex;
    UIFunctionState functions[8];
};
