#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

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
  enum class DisplayMode { MENU, TERMINAL };
  enum class Key { None, Up, Down, Back, Confirm };

  LcdMenu();
  bool init(const char* menuDef);

  void action(Key key);
  void refresh();
  void reset();

  void setEventListener(IMenuEventListener* l);
  void removeEventListener(IMenuEventListener* l);

  void setDynamicProvider(IMenuItemSelectedEventListener* dcp) { _dynamicContentProvider = dcp; }
  void removeDynamicProvider() { _dynamicContentProvider = nullptr; }

  void addTerminalLine(const String& line);
  void clearTerminal();
  DisplayMode setDisplayMode(DisplayMode newMode);

private:
  JsonDocument _menuDoc;
  MenuNode*    _menuRoot;
  MenuNode*    _activeItem;

  std::vector<String> _terminalLines;
  std::vector<IMenuEventListener*> _eventListeners;
  IMenuItemSelectedEventListener* _dynamicContentProvider;
  DisplayMode _displayMode;

  void cleanup();
  MenuNode* buildMenuTree(JsonArray menuItems, MenuNode* parent);
  void render();
  void notifyEventListeners(const JsonVariantConst eventData);
};
