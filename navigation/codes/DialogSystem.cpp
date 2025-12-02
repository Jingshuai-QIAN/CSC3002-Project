#include "DialogSystem.h"
#include <SFML/Graphics.hpp>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "Utils/Logger.h"

// ======================================================
// ✅ 新增：文本自动换行函数（从DialogSystem.cpp复制）
// ======================================================
static std::string wrapText(
    const std::string& str,
    const sf::Font& font,
    unsigned int characterSize,
    float maxWidth
) {
    sf::Text measure(font, "", characterSize);
    float spaceWidth = measure.getFont().getGlyph(' ', characterSize, false).advance;

    std::string result;
    std::string word;
    float lineWidth = 0.f;

    auto wordWidthCalc = [&](const std::string& w)->float {
        float ww = 0.f;
        for (unsigned char ch : w) {
            ww += measure.getFont().getGlyph(ch, characterSize, false).advance;
        }
        return ww;
    };

    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (c == ' ' || c == '\n') {
            if (!word.empty()) {
                float w = wordWidthCalc(word);
                if (lineWidth > 0.f && lineWidth + w > maxWidth) {
                    result.push_back('\n');
                    lineWidth = 0.f;
                } else if (lineWidth > 0.f) {
                    result.push_back(' ');
                    lineWidth += spaceWidth;
                }
                result += word;
                lineWidth += w;
                word.clear();
            }
            if (c == '\n') {
                result.push_back('\n');
                lineWidth = 0.f;
            }
        } else {
            word.push_back(c);
        }
    }

    if (!word.empty()) {
        float w = wordWidthCalc(word);
        if (lineWidth > 0.f && lineWidth + w > maxWidth) {
            result.push_back('\n');
        } else if (lineWidth > 0.f) {
            result.push_back(' ');
        }
        result += word;
    }

    return result;
}

void DialogSystem::initialize(const std::string& bgPath, 
                              const std::string& btnPath, 
                              const sf::Font& font, 
                              unsigned int fontSize) {
    m_fontSize = fontSize;
    m_font = font; // 保存字体引用

    // 1. 加载背景纹理 + 创建带纹理的 Sprite
    if (!m_bgTexture.loadFromFile(bgPath)) {
        throw std::runtime_error("Failed to load dialog bg: " + bgPath);
    }
    // 核心：确保 m_dialogBgSprite 关联纹理
    m_dialogBgSprite = std::make_unique<sf::Sprite>(m_bgTexture);
    m_dialogSize = sf::Vector2f(m_bgTexture.getSize().x, m_bgTexture.getSize().y);
    
    // 2. 加载按钮纹理 + 创建带纹理的 Sprite
    if (!m_btnTexture.loadFromFile(btnPath)) {
        throw std::runtime_error("Failed to load dialog btn: " + btnPath);
    }
    m_btnSprite = std::make_unique<sf::Sprite>(m_btnTexture);
    m_btnTex = m_btnTexture;
    m_btnSize = sf::Vector2f(m_btnTexture.getSize().x, m_btnTexture.getSize().y);
    
    m_dialogTitle.setFont(m_font);
    m_dialogTitle.setCharacterSize(m_fontSize);
    m_dialogTitle.setFillColor(sf::Color::White);
    m_dialogTitle.setLineSpacing(1.2f); // 新增：设置行间距
}

// 新增：计算对话框居中位置（核心）
sf::Vector2f DialogSystem::getCenteredPosition(const sf::RenderWindow& window) {
    if (!m_dialogBgSprite) return {0, 0}; // 空指针保护

    sf::Vector2u windowSize = window.getSize();
    float windowW = static_cast<float>(windowSize.x);
    float windowH = static_cast<float>(windowSize.y);
    
    // 缩放对话框到窗口宽度的 60%（SFML 3.0 setScale 传 Vector2f）
    float scaleFactor = windowW * 0.6f / m_dialogSize.x;
    m_dialogBgSprite->setScale(sf::Vector2f(scaleFactor, scaleFactor)); // 修复：传 Vector2f
    
    // 更新缩放后的尺寸（SFML 3.0 用 size.x/size.y 替代 width/height）
    sf::Vector2f scaledDialogSize = sf::Vector2f(
        m_dialogSize.x * scaleFactor,
        m_dialogSize.y * scaleFactor
    );
    
    // 居中计算
    float posX = (windowW - scaledDialogSize.x) / 2.0f;
    float posY = (windowH - scaledDialogSize.y) / 2.0f;
    
    return sf::Vector2f(posX, posY);
}

// 第一步：实现对话框居中辅助方法（DialogSystem.cpp）
sf::Vector2f DialogSystem::getDialogCenterPosition(const sf::RenderWindow& window) {
    if (!m_dialogBgSprite || m_bgTexture.getSize().x == 0) {
        std::cerr << "Dialog sprite/texture not initialized!" << std::endl;
        return {0, 0};
    }

    // 1. 获取窗口默认视图的尺寸（全屏）
    sf::Vector2u windowSize = window.getSize();
    float windowW = static_cast<float>(windowSize.x);
    float windowH = static_cast<float>(windowSize.y);

    // 2. 强制缩放：对话框宽度 = 窗口宽度的 50%（优先保证不超出）
    float targetWidth = windowW * 0.5f;
    sf::Vector2u texSize = m_bgTexture.getSize();
    float originalW = static_cast<float>(texSize.x);
    float originalH = static_cast<float>(texSize.y);

    // 等比例缩放（避免拉伸）
    float scaleFactor = targetWidth / originalW;
    // 限制缩放范围（最小 0.3，最大 1.0）
    scaleFactor = std::max(0.3f, std::min(scaleFactor, 1.0f));

    // 3. 应用缩放
    m_dialogBgSprite->setScale(sf::Vector2f(scaleFactor, scaleFactor));

    // 4. 计算绝对居中（窗口中心 - 对话框中心）
    sf::FloatRect scaledBounds = m_dialogBgSprite->getGlobalBounds();
    float posX = (windowW - scaledBounds.size.x) / 2.0f;
    float posY = (windowH - scaledBounds.size.y) / 2.0f;

    return {posX, posY};
}

// 修复：获取背景尺寸（适配 SFML 3.0 FloatRect）
sf::Vector2f DialogSystem::getDialogBgSize() const {
    if (!m_dialogBgSprite) {
        return {450, 300}; // 兜底默认尺寸
    }
    // 返回缩放后的实际尺寸（而非纹理原始尺寸）
    const sf::FloatRect bgBounds = m_dialogBgSprite->getGlobalBounds();
    return {bgBounds.size.x, bgBounds.size.y};
}

// ======================================================
// ✅ 新增：带索引回调的方法（从DialogSystem.cpp复制）
// ======================================================
void DialogSystem::setDialogWithIndex(const std::string& title,
                                      const std::vector<std::string>& options,
                                      const OptionCallback& selectCallback) {
    m_isActive = true;
    m_optionCallback = selectCallback;
    m_simpleCallback = nullptr;  // 清除旧的回调
    m_useIndexCallback = true;   // 使用新的回调
    m_buttons.clear();

    m_dialogTitle.setFont(m_font);
    m_dialogTitle.setCharacterSize(m_fontSize);
    m_dialogTitle.setFillColor(sf::Color::White);
    m_dialogTitle.setLineSpacing(1.2f);
    m_dialogTitle.setString(title);

    // 创建按钮 - 使用带索引的回调
    for (size_t i = 0; i < options.size(); ++i) {
        const auto& option = options[i];
        Button btn(m_font, m_fontSize);
        btn.text = option;
        
        // 带索引的回调：传递选项索引和文本
        btn.callback = [selectCallback, i, option]() {
            if (selectCallback) {
                selectCallback(static_cast<int>(i), option);
            }
        };

        btn.textObj.setFont(m_font);
        btn.textObj.setCharacterSize(m_fontSize);
        btn.textObj.setString(option);
        btn.textObj.setFillColor(sf::Color::Black);

        btn.sprite = std::make_unique<sf::Sprite>(m_btnTex);
        m_buttons.push_back(std::move(btn));
    }
}

void DialogSystem::setDialog(const std::string& title,
                             const std::vector<std::string>& dishOptions,
                             const std::function<void(const std::string&)>& selectCallback) {
    m_isActive = true;
    m_simpleCallback = selectCallback; // 修改：使用新的成员变量名
    m_optionCallback = nullptr;  // 清除新的回调
    m_useIndexCallback = false;  // 使用旧的回调
    m_buttons.clear();

    if (!m_dialogBgSprite) return; // 确保背景已初始化

    // ========== 新增：提前设置对话框居中位置（用兜底窗口尺寸）（来自你的代码） ==========
    sf::Vector2f dummyWindowSize(1200, 800); 
    float targetWidth = dummyWindowSize.x * 0.5f;
    float scaleFactor = targetWidth / static_cast<float>(m_bgTexture.getSize().x);
    scaleFactor = std::max(0.3f, std::min(scaleFactor, 1.0f));
    m_dialogBgSprite->setScale(sf::Vector2f(scaleFactor, scaleFactor));
    
    // 提前计算居中位置并设置
    sf::Vector2f centeredPos(
        (dummyWindowSize.x - m_dialogBgSprite->getGlobalBounds().size.x) / 2,
        (dummyWindowSize.y - m_dialogBgSprite->getGlobalBounds().size.y) / 2
    );
    m_dialogBgSprite->setPosition(centeredPos);

    // 设置标题
    m_dialogTitle.setFont(m_font);
    m_dialogTitle.setString(title);
    m_dialogTitle.setCharacterSize(m_fontSize);
    m_dialogTitle.setFillColor(sf::Color::White);
    m_dialogTitle.setLineSpacing(1.2f);

    // 获取背景位置和尺寸（适配 SFML 3.0）
    sf::Vector2f dialogPos = m_dialogBgSprite->getPosition();
    sf::Vector2f bgSize = getDialogBgSize();

    // 创建菜品按钮（适配 SFML 3.0）
    for (const auto& dish : dishOptions) {
        Button btn(m_font, m_fontSize);
        btn.text = dish;
        
        // 简单回调：只传递文本
        btn.callback = [this, dish]() {
            if (m_simpleCallback) {
                m_simpleCallback(dish);
            }
        };

        // 设置按钮文字
        btn.textObj.setFont(m_font);
        btn.textObj.setCharacterSize(m_fontSize);
        btn.textObj.setString(dish);
        btn.textObj.setFillColor(sf::Color::Black);
        
        // 创建按钮 Sprite（SFML 3.0 必须传纹理）
        btn.sprite = std::make_unique<sf::Sprite>(m_btnTex);
        
        m_buttons.push_back(std::move(btn));
    }

    // 布局按钮
    layoutButtons();
}

// ======================================================
// ✅ 新增：根据标题动态布局按钮（防止遮挡）- 修复版本
// ======================================================
void DialogSystem::layoutButtons() {
    if (m_buttons.empty() || !m_dialogBgSprite) return;

    sf::Vector2f dialogPos = m_dialogBgSprite->getPosition();
    sf::Vector2f bgSize = getDialogBgSize();

    float btnSpacing = 15.0f;
    float btnWidthRatio = 0.7f;

    // 计算按钮起始位置 - 基于标题文本的底部
    float btnTopOffset = 80.0f; // 默认偏移
    
    // 如果标题有内容，根据标题的实际底部位置计算
    if (!m_dialogTitle.getString().isEmpty()) {
        sf::FloatRect titleGlobal = m_dialogTitle.getGlobalBounds();
        float titleBottom = titleGlobal.position.y + titleGlobal.size.y;
        btnTopOffset = (titleBottom - dialogPos.y) + 20.0f; // 标题底部 + 间距
    }

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        Button& btn = m_buttons[i];
        if (!btn.sprite) continue;

        float targetBtnWidth = bgSize.x * btnWidthRatio;
        sf::Vector2u btnTexSize = m_btnTexture.getSize();
        float btnScale = targetBtnWidth / static_cast<float>(btnTexSize.x);
        btnScale = std::clamp(btnScale, 0.5f, 1.0f);
        btn.sprite->setScale(sf::Vector2f(btnScale, btnScale));

        sf::FloatRect btnBounds = btn.sprite->getGlobalBounds();
        float btnX = dialogPos.x + (bgSize.x - btnBounds.size.x) / 2.0f;
        float btnY = dialogPos.y + btnTopOffset + i * (btnBounds.size.y + btnSpacing);

        btn.sprite->setPosition(sf::Vector2f(btnX, btnY));
        btn.bounds = btn.sprite->getGlobalBounds();

        // 按钮文本居中
        sf::FloatRect textBounds = btn.textObj.getLocalBounds();
        btn.textObj.setOrigin(sf::Vector2f(
            textBounds.position.x + textBounds.size.x / 2.0f,
            textBounds.position.y + textBounds.size.y / 2.0f
        ));
        btn.textObj.setPosition(sf::Vector2f(
            btnX + btnBounds.size.x / 2.0f,
            btnY + btnBounds.size.y / 2.0f
        ));
    }
}

// ======================================================
// ✅ 修改：事件处理（点击 + ESC + Hover）- 安全版本
// ======================================================
void DialogSystem::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (!m_isActive) return;

    // ========== 1. 处理鼠标左键点击（SFML 3.0 接口） ==========
    if (event.is<sf::Event::MouseButtonPressed>()) {
        // 获取鼠标点击事件详情
        const auto& mouseEvent = *event.getIf<sf::Event::MouseButtonPressed>();
        // 检测是否是左键（SFML 3.0 枚举：sf::Mouse::Button::Left）
        if (mouseEvent.button == sf::Mouse::Button::Left) {
            // 1. 获取鼠标原始像素坐标
            sf::Vector2i mousePixelPos(mouseEvent.position.x, mouseEvent.position.y);
            // 2. 转换到窗口默认视图的坐标（对话框渲染的视图）
            sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos, window.getDefaultView());
            
            std::function<void()> pendingCallback = nullptr;

            // 3. 遍历按钮检测点击
            for (auto& btn : m_buttons) {
                if (!btn.sprite) continue;
                // 按钮的 bounds 是基于默认视图的坐标，直接检测
                if (btn.bounds.contains(mouseWorldPos)) {
                    if (btn.callback)
                        pendingCallback = btn.callback;

                    m_isActive = false;
                    break;
                }
            }

            if (pendingCallback) {
                // ✅ ✅ ✅ 【关键修复】
                // 不要在这里清空 m_simpleCallback！
                // 否则 pendingCallback 执行时就是"空回调"

                m_buttons.clear();           // 清按钮是安全的
                m_pendingCallback = pendingCallback;  // 转交给主循环执行
            }
        }
    }

    // ========== 2. 处理ESC键关闭（SFML 3.0 接口） ==========
    if (event.is<sf::Event::KeyPressed>()) {
        const auto& keyEvent = *event.getIf<sf::Event::KeyPressed>();
        if (keyEvent.code == sf::Keyboard::Key::Escape) {
            m_isActive = false;
        }
    }

    // ========== 可选：鼠标Hover效果（SFML 3.0 接口） ==========
    if (event.is<sf::Event::MouseMoved>()) {
        const auto& mouseMoveEvent = *event.getIf<sf::Event::MouseMoved>();
        sf::Vector2i mousePixelPos(mouseMoveEvent.position.x, mouseMoveEvent.position.y);
        sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos, window.getDefaultView());
        
        for (auto& btn : m_buttons) {
            if (!btn.sprite) continue;
            if (btn.bounds.contains(mouseWorldPos)) {
                btn.sprite->setColor(sf::Color(220, 220, 220)); // 高亮
            } else {
                btn.sprite->setColor(sf::Color::White); // 恢复
            }
        }
    }
}

// ======================================================
// ✅ 新增：回调处理相关方法
// ======================================================
bool DialogSystem::hasPendingCallback() const {
    return m_pendingCallback != nullptr;
}

std::function<void()> DialogSystem::consumePendingCallback() {
    auto callback = m_pendingCallback;
    m_pendingCallback = nullptr;
    return callback;
}

// ======================================================
// ✅ 新增：关闭对话框方法
// ======================================================
void DialogSystem::close() {
    m_isActive = false;
    m_buttons.clear();
    m_simpleCallback = nullptr;
    m_optionCallback = nullptr;
    m_pendingCallback = nullptr; // 清除待处理回调
}

// 修改：渲染逻辑（适配居中+尺寸）
void DialogSystem::render(sf::RenderWindow& window) {
    if (!m_isActive || !m_dialogBgSprite) return;

    // ========== 关键修复：保存当前视图 + 切换到默认视图 ==========
    sf::View originalView = window.getView(); // 保存游戏相机视图
    window.setView(window.getDefaultView()); // 切换到窗口默认视图（全屏坐标）

    // 1. 强制更新缩放 + 居中位置（基于窗口默认视图）
    sf::Vector2f centeredPos = getDialogCenterPosition(window);
    m_dialogBgSprite->setPosition(centeredPos);
    
    // 2. 重新计算背景尺寸（缩放后）
    sf::Vector2f bgSize = getDialogBgSize();
    
    // ========== 新增：自动换行处理 ==========
    float padding = 30.f;
    float maxTextWidth = bgSize.x - padding * 2.0f;
    std::string currentTitle = m_dialogTitle.getString().toAnsiString();
    std::string wrappedTitle = wrapText(currentTitle, m_font, m_fontSize, maxTextWidth);
    m_dialogTitle.setString(wrappedTitle);
    
    // 计算文本位置 - 修复多行文本布局
    sf::FloatRect titleBounds = m_dialogTitle.getLocalBounds();
    
    // 设置文本位置（居中，但考虑多行高度）
    m_dialogTitle.setPosition(sf::Vector2f(
        centeredPos.x + bgSize.x / 2.0f,  // 水平居中
        centeredPos.y + 30.0f             // 距离顶部固定距离
    ));
    
    // 设置原点为中心点，便于居中
    m_dialogTitle.setOrigin(sf::Vector2f(
        titleBounds.size.x / 2.0f + titleBounds.position.x,
        titleBounds.size.y / 2.0f + titleBounds.position.y
    ));

    // 4. 重新布局按钮（适配最新的对话框位置和尺寸）
    layoutButtons();

    // 5. 绘制元素
    window.draw(*m_dialogBgSprite);
    window.draw(m_dialogTitle);
    for (const auto& btn : m_buttons) {
        if (btn.sprite) {
            window.draw(*btn.sprite);
        }
        window.draw(btn.textObj);
    }

    // ========== 关键修复：恢复游戏相机视图 ==========
    window.setView(originalView);
}
