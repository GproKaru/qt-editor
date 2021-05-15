#include "PropertyTreeWidget.hpp"
#include "PropertyTreeItemBase.hpp"
#include "model.hpp"
#include "resizeablearrowitem.hpp"
#include "resizeablerectitem.hpp"
#include "StringProperty.hpp"
#include "BasicTypeProperty.hpp"
#include "EnumProperty.hpp"
#include <QHeaderView>

PropertyTreeItemBase* PropertyTreeWidget::FindObjectPropertyByKey(const void* pKey)
{
    for (int i = 0; i < topLevelItemCount(); i++)
    {
        auto pItem = static_cast<PropertyTreeItemBase*>(topLevelItem(i));
        const void* itemKey = pItem->GetPropertyLookUpKey();
        if (itemKey == pKey)
        {
            return pItem;
        }
    }
    return nullptr;
}

void PropertyTreeWidget::Populate(Model& model, QUndoStack& undoStack, QGraphicsItem* pItem)
{
    auto pLine = qgraphicsitem_cast<ResizeableArrowItem*>(pItem);
    auto pRect = qgraphicsitem_cast<ResizeableRectItem*>(pItem);

    QList<QTreeWidgetItem*> items;
    QTreeWidgetItem* parent = nullptr;
    if (pRect)
    {
        MapObject* pMapObject = pRect->GetMapObject();

        items.append(new StringProperty(undoStack, parent, kIndent + "Name", &pMapObject->mName));
        AddProperties(model, undoStack, items, pMapObject->mProperties, pItem);
    }
    else if (pLine)
    {
        CollisionObject* pCollisionItem = pLine->GetCollisionItem();
        AddProperties(model, undoStack, items, pCollisionItem->mProperties, pItem);
    }

#ifdef _WIN32
    for (int i = 0; i < items.count(); i++)
    {
        const int b = (i % 2) == 0 ? 191 : 222;
        items[i]->setBackground(0, QColor(255, 255, b));
        items[i]->setBackground(1, QColor(255, 255, b));

    }
#endif

    insertTopLevelItems(0, items);
}

void PropertyTreeWidget::Init()
{
    // Two columns, property and value
    setColumnCount(2);

    // Set the header text
    QStringList headerStrings;
    headerStrings.append("Property");
    headerStrings.append("Value");
    setHeaderLabels(headerStrings);

    setAlternatingRowColors(true);
    setStyleSheet("QTreeView::item { height:23px; font:6px; padding:0px; margin:0px; }");

    header()->resizeSection(0, 200);
    header()->resizeSection(1, 90);

    setUniformRowHeights(true);

    setRootIsDecorated(false);

    connect(this, &QTreeWidget::currentItemChanged, this, [&](QTreeWidgetItem* current, QTreeWidgetItem* prev)
        {
            if (prev)
            {
                setItemWidget(prev, 1, nullptr);
            }

            if (current)
            {
                setItemWidget(current, 1, static_cast<PropertyTreeItemBase*>(current)->CreateEditorWidget(this));
            }
        });
}

void PropertyTreeWidget::AddProperties(Model& model, QUndoStack& undoStack, QList<QTreeWidgetItem*>& items, std::vector<UP_ObjectProperty>& props, QGraphicsItem* pGraphicsItem)
{
    QTreeWidgetItem* parent = nullptr;
    for (UP_ObjectProperty& property : props)
    {
        if (property->mVisible)
        {
            switch (property->mType)
            {
            case ObjectProperty::Type::BasicType:
            {
                BasicType* pBasicType = model.FindBasicType(property->mTypeName);
                items.append(new BasicTypeProperty(undoStack, parent, kIndent + property->mName.c_str(), property.get(), pGraphicsItem, pBasicType));
            }
                break;

            case ObjectProperty::Type::Enumeration:
            {
                Enum* pEnum = model.FindEnum(property->mTypeName);
                items.append(new EnumProperty(undoStack, parent, property.get(), pGraphicsItem, pEnum));
            }
                break;
            }
        }
    }
}
