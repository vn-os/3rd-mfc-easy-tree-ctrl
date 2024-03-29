#include "pch.h"
#include "EasyTreeCtrl.h"

#include <cassert>

#pragma warning(disable : 26454)

IMPLEMENT_DYNAMIC(EasyTreeCtrl, CTreeCtrl)

BEGIN_MESSAGE_MAP(EasyTreeCtrl, CTreeCtrl)
  ON_WM_DESTROY()
  ON_WM_CONTEXTMENU()
  ON_COMMAND_RANGE(ID_CONTEXT_MENU_INSERT, ID_CONTEXT_MENU_DELETE, OnContextMenuHandler)

  ON_NOTIFY_REFLECT(NM_CLICK,  OnLClick)
  ON_NOTIFY_REFLECT(NM_RCLICK, OnRClick)
  ON_NOTIFY_REFLECT(NM_DBLCLK, OnDblclk)
  ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelChanged)

  ON_NOTIFY_REFLECT(TVN_BEGINLABELEDIT, OnBeginLabelEdit)
  ON_NOTIFY_REFLECT(TVN_ENDLABELEDIT, OnEndLabelEdit)
END_MESSAGE_MAP()

EasyTreeCtrl::EasyTreeCtrl(CDialog* pParent) : CTreeCtrl(), m_pParent(pParent), m_pfnNotify(nullptr)
{
}

EasyTreeCtrl::~EasyTreeCtrl()
{
}

void EasyTreeCtrl::OnDestroy()
{
  this->Clear();
}

void EasyTreeCtrl::OnContextMenu(CWnd* pWnd, CPoint point)
{
  if (point.x == -1 && point.y == -1) // SHIFT F10
  {
    DWORD position = GetMessagePos();
    point.x = GET_X_LPARAM(position);
    point.y = GET_Y_LPARAM(position);
  }

  // manually select a new item that changed by right-click

  if (auto pItem = this->GetpItemFocusing())
  {
    this->SelectItem(pItem);
  }

  // display context menu

  auto fnGetMenuStateFlags = [&](UINT ID, eNotifyType Before) -> UINT
  {
    UINT result = MF_STRING | MF_POPUP;

    auto pItem = this->GetSelectedItem();
    if (!this->Notify(Before, pItem, UIntToPtr(ID)))
    {
      result |= MF_GRAYED | MF_DISABLED;
    }

    return result;
  };

  #define APPEND_MENU_ITEM(menu, name, id, action) menu.AppendMenu(fnGetMenuStateFlags(id, action), id, L ## name)

  CMenu context_menu;
  context_menu.CreatePopupMenu();
  APPEND_MENU_ITEM(context_menu, "Insert", ID_CONTEXT_MENU_INSERT, eNotifyType::BEFORE_INSERTING);
  APPEND_MENU_ITEM(context_menu, "Modify", ID_CONTEXT_MENU_MODIFY, eNotifyType::BEFORE_MODIFYING);
  APPEND_MENU_ITEM(context_menu, "Delete", ID_CONTEXT_MENU_DELETE, eNotifyType::BEFORE_DELETING);
  context_menu.TrackPopupMenu(TPM_LEFTALIGN, point.x, point.y, this);
}

void EasyTreeCtrl::OnContextMenuHandler(UINT ID)
{
  auto pItem = this->GetSelectedItem();
  auto pOptinal = UIntToPtr(ID);

  switch (ID)
  {
  case ID_CONTEXT_MENU_INSERT:
    {
      if (this->Notify(eNotifyType::BEFORE_INSERTING, pItem, pOptinal))
      {
        this->Notify(eNotifyType::AFTER_INSERTING, pItem, pOptinal);
      }
    }
    break;

  case ID_CONTEXT_MENU_MODIFY:
    {
      if (pItem != nullptr)
      {
        this->EditItem(pItem);
      }
    }
    break;

  case ID_CONTEXT_MENU_DELETE:
    {
      if (pItem != nullptr)
      {
        if (this->Notify(eNotifyType::BEFORE_DELETING, pItem, pOptinal))
        {
          auto pNode = reinterpret_cast<Node*>(this->GetItemData(pItem));

          this->Cleanup(this->GetChildItem(pItem)); // cleanup all child items before deleting
          this->DeleteItem(pItem);
          this->Notify(eNotifyType::AFTER_DELETING, pNode, pOptinal);

          if (pNode != nullptr) // after notify after deleting, cleanup the item
          {
            delete pNode;
          }
        }
      }
    }
    break;

  default:
    break;
  }
}

void EasyTreeCtrl::OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
  auto pNMTreeView = (NM_TREEVIEW*)pNMHDR;
  assert(pNMTreeView != nullptr);

  auto pNode = reinterpret_cast<Node*>(this->GetItemData(pNMTreeView->itemNew.hItem));
  if (pNode != nullptr)
  {
    this->Notify(eNotifyType::SELECTING, pNode);
  }
}

void EasyTreeCtrl::OnLClick(NMHDR* pNMHDR, LRESULT* pResult)
{
  *pResult = FALSE;

  UINT flags = 0;
  auto pItem = this->GetpItemFocusing(&flags);
  if (pItem != nullptr)
  {
    if (flags & TVHT_ONITEMSTATEICON)
    {
      this->Select(pItem, TVGN_CARET);
      this->Notify(eNotifyType::BOX_CHECKING, pItem);
      *pResult = TRUE;
    }
  }
}

void EasyTreeCtrl::OnRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
  CPoint point;
  GetCursorPos(&point);

  this->SendMessage(WM_CONTEXTMENU, WPARAM(GetSafeHwnd()), MAKELPARAM(point.x, point.y));

  *pResult = TRUE;
}

void EasyTreeCtrl::OnDblclk(NMHDR* pNMHDR, LRESULT* pResult)
{
  auto pItem = this->GetpItemFocusing();
  assert(pItem != nullptr);
  
  this->EditItem(pItem);
  
  *pResult = TRUE;
}

void EasyTreeCtrl::OnBeginLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
  *pResult = FALSE;
}

void EasyTreeCtrl::OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
  auto pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
  assert(pTVDispInfo != nullptr);

  auto pItem = &pTVDispInfo->item;
  assert(pItem != nullptr);

  if (pItem->pszText != nullptr)
  {
    if (this->Notify(eNotifyType::BEFORE_MODIFYING, pItem->hItem))
    {
      pItem->mask = TVIF_TEXT;
      this->SetItem(pItem);
      this->Notify(eNotifyType::AFTER_MODIFYING, pItem->hItem);
    }
  }

  *pResult = FALSE;
}

void EasyTreeCtrl::OnNotify(FnNotify pfn)
{
  m_pfnNotify = pfn;
}

BOOL EasyTreeCtrl::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN)
  {
    if (pMsg->wParam == VK_RETURN)
    {
      this->EndEditLabelNow(FALSE);
      return TRUE;
    }
    else if (pMsg->wParam == VK_ESCAPE)
    {
      this->EndEditLabelNow(TRUE);
      return TRUE;
    }
  }

  return __super::PreTranslateMessage(pMsg);
}

bool EasyTreeCtrl::Notify(eNotifyType action, HTREEITEM pItem, void* pOptional)
{
  Node* pNode = nullptr;

  if (pItem != nullptr)
  {
    pNode = reinterpret_cast<Node*>(this->GetItemData(pItem));
    if (pNode != nullptr)
    {
      TVITEM tv = { 0 };
      tv.hItem = pItem;
      tv.mask = TVIF_TEXT;
      tv.stateMask = TVIF_TEXT;

      WCHAR s[MAXBYTE] = { 0 };
      tv.pszText = LPWSTR(&s);
      tv.cchTextMax = ARRAYSIZE(s);

      this->GetItem(&tv);
      pNode->m_ptr_tv = &tv;
    }
  }

  return this->Notify(action, pNode, pOptional);
}

bool EasyTreeCtrl::Notify(eNotifyType action, Node* pNode, void* pOptional)
{
  if (m_pfnNotify == nullptr)
  {
    return true;
  }

  return m_pfnNotify(action, pNode, pOptional);
}

void EasyTreeCtrl::Populate(std::function<void(HTREEITEM& root)> pfn, const CString& name)
{
  if (pfn != nullptr)
  {
    auto root = this->InsertItem(name, TVI_ROOT);
    pfn(root);
    this->Expand(root, TVE_EXPAND);
  }
}

HTREEITEM EasyTreeCtrl::InsertNode(HTREEITEM& pParent, Node* pNode)
{
  assert(pParent != nullptr && pNode != nullptr);

  auto item = this->InsertItem(pNode->m_name, pParent);
  this->SetItemData(item, DWORD_PTR(pNode));

  return item;
}

HTREEITEM EasyTreeCtrl::GetpItemFocusing(UINT* pFlags)
{
  CPoint point;
  GetCursorPos(&point);
  this->ScreenToClient(&point);
  return this->HitTest(point, pFlags);
}

bool EasyTreeCtrl::EditItem(HTREEITEM pItem)
{
  if (pItem == nullptr)
  {
    return false;
  }

  if (this->EndEditLabelNow(TRUE))
  {
    this->SelectItem(pItem);
    this->EditLabel(pItem);
  }

  return true;
}

void EasyTreeCtrl::Cleanup(HTREEITEM pItem)
{
  this->Iterate(pItem, [&](HTREEITEM pItem) -> void
  {
    if (auto pNode = reinterpret_cast<Node*>(this->GetItemData(pItem)))
    {
      delete pNode;
    }
  });
}

void EasyTreeCtrl::Clear()
{
  this->Cleanup(this->GetRootItem());
  this->DeleteAllItems();
}

void EasyTreeCtrl::Iterate(HTREEITEM hItem, std::function<void(HTREEITEM pItem)> pfn)
{
  if (hItem == nullptr || pfn == nullptr)
  {
    return;
  }

  do
  {
    pfn(hItem);

    if (this->ItemHasChildren(hItem))
    {
      this->Iterate(this->GetChildItem(hItem), pfn);
    }

  } while (hItem = this->GetNextSiblingItem(hItem));
}
