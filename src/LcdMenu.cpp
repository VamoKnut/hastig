#include "LcdMenu.h"

#include "Display.h"

#include <algorithm>
#include <string.h>

LcdMenu::LcdMenu()
    : _menuRoot(nullptr), _activeItem(nullptr), _dynamicContentProvider(nullptr),
      _displayMode(DisplayMode::MENU)
{
}

bool LcdMenu::init(const char* menuDef)
{
   cleanup();

   if (menuDef == nullptr)
   {
      return false;
   }

   const DeserializationError err = deserializeJson(_menuDoc, menuDef);
   if (err)
   {
      return false;
   }

   JsonArray topLevelArray = _menuDoc["items"].as<JsonArray>();
   if (topLevelArray.isNull() || topLevelArray.size() == 0)
   {
      return false;
   }

   _menuRoot = _activeItem = buildMenuTree(topLevelArray, nullptr);
   if (_menuRoot == nullptr)
   {
      return false;
   }

   render();
   return true;
}

LcdMenu::DisplayMode LcdMenu::setDisplayMode(DisplayMode newMode)
{
   const DisplayMode oldMode = _displayMode;
   _displayMode              = newMode;
   render();
   return oldMode;
}

void LcdMenu::cleanup()
{
   _menuDoc.clear();
   if (_menuRoot != nullptr)
   {
      delete _menuRoot;
   }
   _menuRoot   = nullptr;
   _activeItem = nullptr;
}

void LcdMenu::reset()
{
   _activeItem = _menuRoot;
   render();
}

MenuNode* LcdMenu::buildMenuTree(JsonArray menuItems, MenuNode* parent)
{
   if (menuItems.isNull() || menuItems.size() == 0)
   {
      return nullptr;
   }

   MenuNode* firstSibling = nullptr;
   MenuNode* lastSibling  = nullptr;

   for (JsonVariant item : menuItems)
   {
      MenuNode* currentNode = new MenuNode(parent);
      currentNode->setMeta(item);

      if (firstSibling == nullptr)
      {
         firstSibling = currentNode;
      }

      if (lastSibling != nullptr)
      {
         lastSibling->next = currentNode;
      }
      currentNode->prev = lastSibling;

      const char* type = item["type"] | "";
      if (strcmp(type, "sub") == 0 || strcmp(type, "sel") == 0)
      {
         currentNode->addChild(buildMenuTree(item["items"].as<JsonArray>(), currentNode));
      }

      lastSibling = currentNode;
   }

   return firstSibling;
}

void LcdMenu::action(Key key)
{
   if (_activeItem == nullptr)
   {
      return;
   }

   if (_displayMode == DisplayMode::TERMINAL)
   {
      if (key == Key::Back || key == Key::Confirm)
      {
         _displayMode = DisplayMode::MENU;
      }
      render();
      return;
   }

   switch (key)
   {
   case Key::Down:
      if (_activeItem->getNextSibling() != nullptr)
      {
         _activeItem = _activeItem->getNextSibling();
      }
      break;
   case Key::Up:
      if (_activeItem->getPrevSibling() != nullptr)
      {
         _activeItem = _activeItem->getPrevSibling();
      }
      break;
   case Key::Confirm:
      if (_activeItem->getChild() != nullptr)
      {
         _activeItem = _activeItem->getChild();
      }
      else
      {
         notifyEventListeners(_activeItem->getMeta()["retval"]);
      }
      break;
   case Key::Back:
      if (_activeItem->getParent() != nullptr)
      {
         _activeItem = _activeItem->getParent();
      }
      break;
   case Key::None:
   default:
      break;
   }

  render();
}

void LcdMenu::refresh()
{
  render();
}

void LcdMenu::render()
{
   if (_activeItem == nullptr)
   {
      return;
   }

   if (_displayMode == DisplayMode::TERMINAL)
   {
      Display::getInstance().terminalTest(_terminalLines, 0);
      return;
   }

   if (_activeItem->isOption())
   {
      Display::getInstance().listSelectableItems(_activeItem, _dynamicContentProvider);
   }
   else
   {
      Display::getInstance().listMenuItems(_activeItem);
   }
}

void LcdMenu::setEventListener(IMenuEventListener* l)
{
   if (l == nullptr)
   {
      return;
   }
   _eventListeners.push_back(l);
}

void LcdMenu::removeEventListener(IMenuEventListener* l)
{
   if (l == nullptr)
   {
      return;
   }
   _eventListeners.erase(std::remove(_eventListeners.begin(), _eventListeners.end(), l),
                         _eventListeners.end());
}

void LcdMenu::notifyEventListeners(const JsonVariantConst eventData)
{
   for (IMenuEventListener* listener : _eventListeners)
   {
      if (listener != nullptr)
      {
         listener->onItemSelectedEvent(eventData);
      }
   }
}

void LcdMenu::addTerminalLine(const String& line)
{
   _terminalLines.push_back(line);
   if (_terminalLines.size() > 16u)
   {
      _terminalLines.erase(_terminalLines.begin());
   }
}

void LcdMenu::clearTerminal()
{
   _terminalLines.clear();
   render();
}

MenuNode::MenuNode(MenuNode* myParent)
    : prev(nullptr), next(nullptr), parent(myParent), children(nullptr), mIsSelected(false)
{
}

MenuNode::~MenuNode()
{
   if (children != nullptr)
   {
      delete children;
      children = nullptr;
   }
   if (next != nullptr)
   {
      delete next;
      next = nullptr;
   }
}

void MenuNode::setMeta(JsonVariant m)
{
   meta = m;
}

MenuNode* MenuNode::addSibling(MenuNode* sibling)
{
   next = sibling;
   return sibling;
}

MenuNode* MenuNode::addChild(MenuNode* child)
{
   children = child;
   return child;
}

unsigned int MenuNode::numSiblings() const
{
   const MenuNode* cursor  = this;
   unsigned int    counter = 1;

   while (cursor->getPrevSibling() != nullptr)
   {
      cursor = cursor->getPrevSibling();
   }

   while (cursor->getNextSibling() != nullptr)
   {
      cursor = cursor->getNextSibling();
      counter++;
   }

   return counter;
}

unsigned int MenuNode::positionInSiblinglist() const
{
   const MenuNode* cursor  = this;
   unsigned int    counter = 1;

   while (cursor->getPrevSibling() != nullptr)
   {
      cursor = cursor->getPrevSibling();
      counter++;
   }
   return counter;
}

String MenuNode::getLabel() const
{
   return meta["label"].as<String>();
}

bool MenuNode::isOption() const
{
   if (getParent() == nullptr)
   {
      return false;
   }
   const char* type = getParent()->meta["type"] | "";
   return strcmp(type, "sel") == 0;
}

bool MenuNode::isSelected() const
{
   return mIsSelected;
}
