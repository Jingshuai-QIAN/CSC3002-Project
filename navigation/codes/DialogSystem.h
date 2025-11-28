#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <functional>

class DialogSystem {
public:
    struct Button {
        std::string text;
        std::function<void()> callback;
        std::unique_ptr<sf::Sprite> sprite;
        sf::Text textObj;
        sf::FloatRect bounds;
        bool isHovered = false;

        // SFML 3.0 必须显式构造 Sprite/Text（无默认构造）
        Button(const sf::Font& font, unsigned int fontSize)
            : textObj(font, "", fontSize)
        {}

        // 禁用拷贝（unique_ptr 不可拷贝）
        Button(const Button&) = delete;
        Button& operator=(const Button&) = delete;

        // 启用移动
        Button(Button&&) = default;
        Button& operator=(Button&&) = default;
    };

    // DialogSystem 构造函数：
    DialogSystem(const sf::Font& font, unsigned int fontSize)
        : m_font(font), m_fontSize(fontSize),
          m_dialogTitle(font, "", fontSize),
          m_textObject(font, "", fontSize),
          m_bgSprite(nullptr),    // 初始化为空
          m_btnSprite(nullptr),
          m_dialogBgSprite(nullptr)
    {}


    // 初始化：加载对话框/按钮素材 + 字体
    void initialize(const std::string& bgPath, 
                    const std::string& btnPath, 
                    const sf::Font& font, 
                    unsigned int fontSize);

    // 设置对话框内容（阿姨提问+菜品选项+选中回调）
    void setDialog(const std::string& title, 
                   const std::vector<std::string>& dishOptions,
                   const std::function<void(const std::string&)>& selectCallback);

    // 处理鼠标/键盘事件（hover/点击/ESC关闭）
    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);

    // 渲染对话框和按钮
    void render(sf::RenderWindow& window);

    // 状态控制
    bool isActive() const { return m_isActive; }
    void close() { m_isActive = false; }



private:
    // 辅助方法（删除重复定义，统一命名）
    sf::Vector2f getCenteredPosition(const sf::RenderWindow& window); // 原缩放居中
    sf::Vector2f getDialogCenterPosition(const sf::RenderWindow& window); // 原基础居中
    sf::Vector2f getDialogBgSize() const; // 获取背景尺寸
    void layoutButtons(); // 按钮布局

    // 状态成员
    bool m_isActive = false;
    const sf::Font& m_font;
    unsigned int m_fontSize;
    std::function<void(const std::string&)> m_selectCallback;

    // 图形资源：只保留纹理，Sprite 在 initialize 中创建（带纹理）
    sf::Texture m_bgTexture;
    std::unique_ptr<sf::Sprite> m_bgSprite;
    sf::Vector2f m_dialogSize;
    std::unique_ptr<sf::Sprite> m_dialogBgSprite;

    sf::Texture m_btnTexture;
    std::unique_ptr<sf::Sprite> m_btnSprite;       // 改用 unique_ptr
    sf::Vector2f m_btnSize;
    sf::Texture m_btnTex;

    // 文本对象
    sf::Text m_dialogTitle;        // 对话框标题
    sf::Text m_textObject;         // 备用文本（你的原变量）
    std::vector<sf::Text> m_optionTexts; // 选项文本（你的原变量）

    std::vector<Button> m_buttons;
};