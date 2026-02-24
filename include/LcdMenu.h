#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <array>
#include <vector>

class IMenuEventListener {
public:
  virtual ~IMenuEventListener() = default;
  virtual void onItemSelectedEvent(const JsonVariantConst itemRetVal) = 0;
};

class IMenuItemSelectedEventListener {
public:
  virtual ~IMenuItemSelectedEventListener() = default;
  virtual bool onIsItemSelectedEvent(const JsonVariantConst itemRetVal) = 0;
  virtual bool getStringSettingValue(const char* prop, String& outValue)
  {
    (void)prop;
    outValue = "";
    return false;
  }
};

class MenuNode
{
public:
  explicit MenuNode(MenuNode* myParent = nullptr);
  ~MenuNode();

  MenuNode* addSibling(MenuNode* sibling);
  MenuNode* addChild(MenuNode* child);
  void      setMeta(JsonVariant m);

  JsonVariant getMeta() const { return meta; }
  String      getLabel() const;

  unsigned int numSiblings() const;
  unsigned int positionInSiblinglist() const;

  MenuNode* getNextSibling() const { return next; }
  MenuNode* getPrevSibling() const { return prev; }
  MenuNode* getParent() const { return parent; }
  MenuNode* getChild() const { return children; }

  bool isOption() const;
  bool isSelected() const;

  MenuNode* prev;
  MenuNode* next;
  MenuNode* parent;
  MenuNode* children;
  bool      mIsSelected;

private:
  JsonVariant meta;
};

class LcdMenu
{
public:
  enum class DisplayMode { MENU, TERMINAL, TEXT_EDITOR };
  enum class Key { None, Up, Down, Back, Confirm };

  LcdMenu();
  bool init(const char* menuDef);

  void action(Key key);
  void refresh();
  void reset();
  bool isAtRootLevel() const;

  void setEventListener(IMenuEventListener* l);
  void removeEventListener(IMenuEventListener* l);

  void setDynamicProvider(IMenuItemSelectedEventListener* dcp) { _dynamicContentProvider = dcp; }
  void removeDynamicProvider() { _dynamicContentProvider = nullptr; }

  void addTerminalLine(const String& line);
  void clearTerminal();
  DisplayMode setDisplayMode(DisplayMode newMode);

private:
  enum class EditorCellAction : uint8_t { None, Append, Delete, Confirm, Cancel, ToggleCase };

  struct EditorCell {
    EditorCellAction action = EditorCellAction::None;
    char             value = '\0';
    char             label[2] = {' ', '\0'};
  };

  static constexpr size_t EDITOR_COLS = 16u;
  static constexpr size_t EDITOR_ROWS = 3u;
  static constexpr size_t EDITOR_CELL_CAPACITY = EDITOR_COLS * EDITOR_ROWS;
  static constexpr size_t EDITOR_DEFAULT_MAX_LEN = 63u;
  static constexpr size_t EDITOR_COMMAND_COUNT = 4u;

  JsonDocument _menuDoc;
  MenuNode*    _menuRoot;
  MenuNode*    _activeItem;

  std::vector<String> _terminalLines;
  std::vector<IMenuEventListener*> _eventListeners;
  IMenuItemSelectedEventListener* _dynamicContentProvider;
  DisplayMode _displayMode;

  String _editorSettingLabel;
  String _editorPropName;
  String _editorValue;
  bool   _editorUpperCase;
  size_t _editorMaxLen;
  size_t _editorCursor;
  size_t _editorSelected;
  size_t _editorCellCount;
  std::array<EditorCell, EDITOR_CELL_CAPACITY> _editorCells;
  std::array<const char*, EDITOR_CELL_CAPACITY> _editorCellLabels;

  void cleanup();
  MenuNode* buildMenuTree(JsonArray menuItems, MenuNode* parent);
  void render();
  void notifyEventListeners(const JsonVariantConst eventData);
  void handleEditorKey(Key key);
  bool enterTextEditor(MenuNode* node);
  void exitTextEditor(bool submit);
  void rebuildEditorCells();
  void clearEditorCells();
  void setEditorCell(size_t index, EditorCellAction action, char value, char label);
  size_t findEditorCellForChar(char c) const;
  void syncSelectionToCursorChar();
  void moveSelection(int delta);
  bool editorAppendChar(char c);
};
