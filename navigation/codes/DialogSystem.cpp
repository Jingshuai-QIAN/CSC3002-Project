#include "DialogSystem.h"
#include <SFML/Graphics.hpp>
#include <stdexcept>
#include <algorithm>
#include <iostream>

void DialogSystem::initialize(const std::string& bgPath, 
                              const std::string& btnPath, 
                              const sf::Font& font, 
                              unsigned int fontSize) {
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
    
    // 3. 初始化文本
    m_textObject.setFont(font);
    m_textObject.setCharacterSize(fontSize);
    m_textObject.setFillColor(sf::Color::Black);
    
    for (auto& optText : m_optionTexts) {
        optText.setFont(font);
        optText.setCharacterSize(fontSize);
        optText.setFillColor(sf::Color::Black);
    }

    m_fontSize = fontSize;
}

// 新增：计算对话框居中位置（核心）
sf::Vector2f DialogSystem::getCenteredPosition(const sf::RenderWindow& window) {
    if (!m_bgSprite) return {0, 0}; // 空指针保护

    sf::Vector2u windowSize = window.getSize();
    float windowW = static_cast<float>(windowSize.x);
    float windowH = static_cast<float>(windowSize.y);
    
    // 缩放对话框到窗口宽度的 60%（SFML 3.0 setScale 传 Vector2f）
    float scaleFactor = windowW * 0.6f / m_dialogSize.x;
    m_bgSprite->setScale(sf::Vector2f(scaleFactor, scaleFactor)); // 修复：传 Vector2f
    
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

void DialogSystem::setDialog(const std::string& title,
                             const std::vector<std::string>& dishOptions,
                             const std::function<void(const std::string&)>& selectCallback) {
    m_isActive = true;
    m_selectCallback = selectCallback;
    m_buttons.clear();

    if (!m_dialogBgSprite) return; // 确保背景已初始化

    // 获取背景位置和尺寸（适配 SFML 3.0）
    sf::Vector2f dialogPos = m_dialogBgSprite->getPosition();
    sf::Vector2f bgSize = getDialogBgSize();

    // 设置标题（修复 SFML 3.0 FloatRect 接口）
    m_dialogTitle.setFont(m_font);
    m_dialogTitle.setString(title);
    m_dialogTitle.setCharacterSize(m_fontSize);
    m_dialogTitle.setFillColor(sf::Color::White);
    
    auto titleBounds = m_dialogTitle.getLocalBounds();
    // SFML 3.0：用 size.x 替代 width，position.x 替代 left
    m_dialogTitle.setPosition(sf::Vector2f(
        dialogPos.x + (bgSize.x - titleBounds.size.x) / 2.0f,
        dialogPos.y + 30.0f
    ));
    // 修复文本原点（SFML 3.0 接口）
    m_dialogTitle.setOrigin(sf::Vector2f(
        titleBounds.position.x + titleBounds.size.x / 2.0f,
        titleBounds.position.y
    ));

    // 创建菜品按钮（适配 SFML 3.0）
    for (const auto& dish : dishOptions) {
        Button btn(m_font, m_fontSize);
        btn.text = dish;
        btn.callback = [this, dish]() {
            m_selectCallback(dish);
            this->close();
        };
        // 设置按钮文字
        btn.textObj.setString(dish);
        btn.textObj.setFillColor(sf::Color::Black);
        // 创建按钮 Sprite（SFML 3.0 必须传纹理）
        btn.sprite = std::make_unique<sf::Sprite>(m_btnTex);
        
        // 按钮缩放（SFML 3.0 setScale 传 Vector2f）
        float btnScale = (bgSize.x * 0.6f) / m_btnTex.getSize().x;
        btn.sprite->setScale(sf::Vector2f(btnScale, btnScale));
        
        // 计算按钮碰撞盒（SFML 3.0 FloatRect）
        btn.bounds = btn.sprite->getGlobalBounds();
        
        m_buttons.push_back(std::move(btn));
    }

    // 布局按钮
    layoutButtons();
}

void DialogSystem::layoutButtons() {
    if (m_buttons.empty() || !m_dialogBgSprite) return;

    sf::Vector2f dialogPos = m_dialogBgSprite->getPosition();
    sf::Vector2f bgSize = getDialogBgSize();
    
    float btnSpacing = 15.0f;
    float btnTopOffset = 80.0f;
    float btnWidthRatio = 0.7f;

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        Button& btn = m_buttons[i];
        if (!btn.sprite) continue;

        // 按钮自适应缩放
        float targetBtnWidth = bgSize.x * btnWidthRatio;
        sf::Vector2u btnTexSize = m_btnTexture.getSize(); // 替换为你实际的按钮纹理变量
        float btnScale = targetBtnWidth / static_cast<float>(btnTexSize.x);
        // 修复：std::clamp → 临时注释（如果编译报错），或保留
        btnScale = std::clamp(btnScale, 0.5f, 1.0f);
        btn.sprite->setScale(sf::Vector2f(btnScale, btnScale));

        // 按钮尺寸（缩放后）
        sf::FloatRect btnBounds = btn.sprite->getGlobalBounds();
        
        // 按钮水平居中
        float btnX = dialogPos.x + (bgSize.x - btnBounds.size.x) / 2.0f;
        // 按钮垂直分布
        float btnY = dialogPos.y + btnTopOffset + i * (btnBounds.size.y + btnSpacing);

        // 设置按钮位置
        btn.sprite->setPosition(sf::Vector2f(btnX, btnY));
        btn.bounds = btn.sprite->getGlobalBounds();
        
        // 按钮文字居中
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
            
            // 3. 遍历按钮检测点击
            for (auto& btn : m_buttons) {
                if (!btn.sprite) continue;
                // 按钮的 bounds 是基于默认视图的坐标，直接检测
                if (btn.bounds.contains(mouseWorldPos)) {
                    // 触发回调
                    if (btn.callback) {
                        btn.callback();
                    }
                    // 关闭对话框
                    m_isActive = false;
                    break;
                }
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

    // 3. 更新标题位置（基于最新的居中位置）
    auto titleBounds = m_dialogTitle.getLocalBounds();
    m_dialogTitle.setPosition(sf::Vector2f(
        centeredPos.x + (bgSize.x - titleBounds.size.x) / 2.0f,
        centeredPos.y + 30.0f // 标题距对话框顶部 30px
    ));
    // 标题原点居中（避免文字偏移）
    m_dialogTitle.setOrigin(sf::Vector2f(
        titleBounds.position.x + titleBounds.size.x / 2.0f,
        titleBounds.position.y
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