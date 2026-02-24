#include "LcdMenu.h"

#include "Display.h"

#include <algorithm>
#include <ctype.h>
#include <string.h>

namespace {
bool nodeTypeEquals(MenuNode* node, const char* type)
{
  if (node == nullptr || type == nullptr) {
    return false;
  }
  const char* nodeType = node->getMeta()["type"] | "";
  return strcmp(nodeType, type) == 0;
}
} // namespace

LcdMenu::LcdMenu()
    : _menuRoot(nullptr),
      _activeItem(nullptr),
      _dynamicContentProvider(nullptr),
      _displayMode(DisplayMode::MENU),
      _editorUpperCase(false),
      _editorMaxLen(EDITOR_DEFAULT_MAX_LEN),
      _editorCursor(0u),
      _editorSelected(0u),
      _editorCellCount(0u)
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
   _editorSettingLabel = "";
   _editorPropName = "";
   _editorValue = "";
   _editorUpperCase = false;
   _editorMaxLen = EDITOR_DEFAULT_MAX_LEN;
   _editorCursor = 0u;
   _editorSelected = 0u;
   _editorCellCount = 0u;
   _displayMode = DisplayMode::MENU;
}

void LcdMenu::reset()
{
   _activeItem = _menuRoot;
   render();
}

bool LcdMenu::isAtRootLevel() const
{
   return (_displayMode == DisplayMode::MENU) &&
          (_activeItem != nullptr) &&
          (_activeItem->getParent() == nullptr);
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

   if (_displayMode == DisplayMode::TEXT_EDITOR)
   {
      handleEditorKey(key);
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
      else if (nodeTypeEquals(_activeItem, "edit"))
      {
         if (!enterTextEditor(_activeItem))
         {
            notifyEventListeners(_activeItem->getMeta()["retval"]);
         }
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

   if (_displayMode == DisplayMode::TEXT_EDITOR)
   {
      Display::getInstance().renderTextEditor(_editorSettingLabel.c_str(),
                                              _editorValue.c_str(),
                                              _editorCellLabels.data(),
                                              _editorCellCount,
                                              _editorSelected,
                                              EDITOR_COLS,
                                              _editorCursor);
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

void LcdMenu::handleEditorKey(Key key)
{
   if (_editorCellCount == 0u)
   {
      exitTextEditor(false);
      return;
   }

   switch (key)
   {
   case Key::Up:
      moveSelection(-1);
      break;
   case Key::Down:
      moveSelection(1);
      break;
   case Key::Back:
      exitTextEditor(false);
      return;
   case Key::Confirm:
   {
      const EditorCell& cell = _editorCells[_editorSelected];
      switch (cell.action)
      {
      case EditorCellAction::Append:
         if (_editorCursor < _editorValue.length())
         {
            if (_editorValue.charAt(_editorCursor) != cell.value)
            {
               _editorValue.setCharAt(_editorCursor, cell.value);
            }
            _editorCursor++;
         }
         else
         {
            if (editorAppendChar(cell.value))
            {
               _editorCursor++;
            }
         }
         syncSelectionToCursorChar();
         break;
      case EditorCellAction::Delete:
         if (_editorValue.length() > 0u && _editorCursor > 0u)
         {
            _editorValue.remove(_editorCursor - 1u, 1u);
            _editorCursor--;
            syncSelectionToCursorChar();
         }
         break;
      case EditorCellAction::ToggleCase:
         _editorUpperCase = !_editorUpperCase;
         rebuildEditorCells();
         break;
      case EditorCellAction::Confirm:
         exitTextEditor(true);
         return;
      case EditorCellAction::Cancel:
         exitTextEditor(false);
         return;
      case EditorCellAction::None:
      default:
         break;
      }
      break;
   }
   case Key::None:
   default:
      break;
   }

   render();
}

bool LcdMenu::editorAppendChar(char c)
{
   if (_editorValue.length() >= _editorMaxLen)
   {
      return false;
   }

   _editorValue += c;
   return true;
}

void LcdMenu::rebuildEditorCells()
{
   clearEditorCells();

   size_t writeIndex = 0u;
   auto placeAppend = [&](char c) {
      if (writeIndex >= (EDITOR_COLS * (EDITOR_ROWS - 1u) + (EDITOR_COLS - EDITOR_COMMAND_COUNT))) {
         return;
      }
      setEditorCell(writeIndex, EditorCellAction::Append, c, c);
      writeIndex++;
   };

   for (char c = _editorUpperCase ? 'A' : 'a'; c <= (_editorUpperCase ? 'Z' : 'z'); ++c) {
      placeAppend(c);
   }
   for (char c = '0'; c <= '9'; ++c) {
      placeAppend(c);
   }
   placeAppend('.');
   placeAppend('-');
   placeAppend('_');
   placeAppend(' ');

   const size_t cmdBase = (EDITOR_ROWS - 1u) * EDITOR_COLS + (EDITOR_COLS - EDITOR_COMMAND_COUNT);
   setEditorCell(cmdBase + 0u, EditorCellAction::Delete, 0, '<');
   setEditorCell(cmdBase + 1u, EditorCellAction::ToggleCase, 0, '^');
   setEditorCell(cmdBase + 2u, EditorCellAction::Confirm, 0, '+');
   setEditorCell(cmdBase + 3u, EditorCellAction::Cancel, 0, 'x');

   _editorCellCount = EDITOR_CELL_CAPACITY;
   syncSelectionToCursorChar();
}

bool LcdMenu::enterTextEditor(MenuNode* node)
{
   if (node == nullptr)
   {
      return false;
   }

   const JsonVariant meta = node->getMeta();
   const JsonVariantConst retVal = meta["retval"];
   const char* prop = retVal["prop"] | "";
   if (prop[0] == '\0')
   {
      return false;
   }

   _editorSettingLabel = node->getLabel();
   _editorPropName = prop;
   _editorValue = "";
   _editorUpperCase = false;
   _editorSelected = 0u;
   _editorMaxLen = EDITOR_DEFAULT_MAX_LEN;
   _editorCursor = 0u;

   if (meta["maxLen"].is<uint32_t>())
   {
      const uint32_t maxLen = meta["maxLen"].as<uint32_t>();
      if (maxLen > 0u)
      {
         _editorMaxLen = (size_t)maxLen;
      }
   }

   if (_dynamicContentProvider != nullptr)
   {
      String currentValue;
      if (_dynamicContentProvider->getStringSettingValue(_editorPropName.c_str(), currentValue))
      {
         _editorValue = currentValue;
      }
   }

   if (_editorValue.length() > _editorMaxLen)
   {
      _editorValue.remove(_editorMaxLen);
   }
   _editorCursor = (_editorValue.length() > 0u) ? 0u : 0u;

   rebuildEditorCells();
   _displayMode = DisplayMode::TEXT_EDITOR;
   render();
   return true;
}

void LcdMenu::exitTextEditor(bool submit)
{
   if (submit && _editorPropName.length() > 0u)
   {
      String trimmedValue = _editorValue;
      while (trimmedValue.length() > 0u &&
             trimmedValue.charAt(trimmedValue.length() - 1u) == ' ')
      {
         trimmedValue.remove(trimmedValue.length() - 1u, 1u);
      }

      JsonDocument doc;
      doc["topic"] = "setup";
      doc["prop"] = _editorPropName;
      doc["value"] = trimmedValue;
      notifyEventListeners(doc.as<JsonVariantConst>());
   }

   _editorSettingLabel = "";
   _editorPropName = "";
   _editorValue = "";
   _editorUpperCase = false;
   _editorMaxLen = EDITOR_DEFAULT_MAX_LEN;
   _editorCursor = 0u;
   _editorSelected = 0u;
   _editorCellCount = 0u;
   _displayMode = DisplayMode::MENU;

   if (_activeItem != nullptr && _activeItem->getParent() != nullptr)
   {
      _activeItem = _activeItem->getParent();
   }

   render();
}

void LcdMenu::clearEditorCells()
{
   for (size_t i = 0u; i < EDITOR_CELL_CAPACITY; ++i)
   {
      _editorCells[i].action = EditorCellAction::None;
      _editorCells[i].value = '\0';
      _editorCells[i].label[0] = ' ';
      _editorCells[i].label[1] = '\0';
      _editorCellLabels[i] = _editorCells[i].label;
   }
}

void LcdMenu::setEditorCell(size_t index, EditorCellAction action, char value, char label)
{
   if (index >= EDITOR_CELL_CAPACITY)
   {
      return;
   }
   _editorCells[index].action = action;
   _editorCells[index].value = value;
   _editorCells[index].label[0] = label;
   _editorCells[index].label[1] = '\0';
   _editorCellLabels[index] = _editorCells[index].label;
}

size_t LcdMenu::findEditorCellForChar(char c) const
{
   for (size_t i = 0u; i < EDITOR_CELL_CAPACITY; ++i)
   {
      if (_editorCells[i].action == EditorCellAction::Append && _editorCells[i].value == c)
      {
         return i;
      }
   }
   return EDITOR_CELL_CAPACITY;
}

void LcdMenu::syncSelectionToCursorChar()
{
   char target = ' ';
   if (_editorCursor < _editorValue.length())
   {
      target = _editorValue.charAt(_editorCursor);
   }
   const size_t idx = findEditorCellForChar(target);
   if (idx != EDITOR_CELL_CAPACITY)
   {
      _editorSelected = idx;
      return;
   }

   if (isalpha((unsigned char)target))
   {
      const bool wantUpper = (target >= 'A' && target <= 'Z');
      if (wantUpper != _editorUpperCase)
      {
         _editorUpperCase = wantUpper;
         rebuildEditorCells();
      }
   }
}

void LcdMenu::moveSelection(int delta)
{
   if (_editorCellCount == 0u)
   {
      return;
   }

   size_t idx = _editorSelected;
   for (size_t n = 0u; n < _editorCellCount; ++n)
   {
      if (delta < 0)
      {
         idx = (idx == 0u) ? (_editorCellCount - 1u) : (idx - 1u);
      }
      else
      {
         idx = (idx + 1u) % _editorCellCount;
      }

      if (_editorCells[idx].action != EditorCellAction::None)
      {
         _editorSelected = idx;
         return;
      }
   }
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
