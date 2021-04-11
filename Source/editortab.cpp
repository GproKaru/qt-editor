#include "editortab.hpp"
#include "ui_editortab.h"
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QOpenGLWidget>
#include <QUndoCommand>
#include <QSpinBox>
#include "resizeablearrowitem.hpp"
#include "resizeablerectitem.hpp"
#include "CameraGraphicsItem.hpp"
#include "EditorGraphicsScene.hpp"

// Zoom by 10% each time.
const float KZoomFactor = 0.10f;
const float KMaxZoomOutLevels = 5.0f;
const float KMaxZoomInLevels = 14.0f;

class SetSelectionCommand : public QUndoCommand
{
public:
    SetSelectionCommand(EditorTab* pTab, QGraphicsScene* pScene, QList<QGraphicsItem*>& oldSelection, QList<QGraphicsItem*>& newSelection) 
      : mTab(pTab),
        mScene(pScene),
        mOldSelection(oldSelection),
        mNewSelection(newSelection)
    {
        mFirst = true;
        if (mNewSelection.count() > 0)
        {
            setText(QString("Select %1 item(s)").arg(mNewSelection.count()));
        }
        else
        {
            setText("Clear selection");
        }
    }

    void redo() override
    {
        if (!mFirst)
        {
            mScene->clearSelection();
            for (auto& item : mNewSelection)
            {
                item->setSelected(true);
            }
            mScene->update();
        }
        mFirst = false;
        SyncPropertyEditor();
    }

    void undo() override
    {
        mScene->clearSelection();
        for (auto& item : mOldSelection)
        {
            item->setSelected(true);
        }
        mScene->update();
        SyncPropertyEditor();
    }

    void SyncPropertyEditor()
    {
        auto selected = mScene->selectedItems();
        if (selected.count() == 1)
        {
            mTab->PopulatePropertyEditor(selected[0]);
        }
        else
        {
            mTab->ClearPropertyEditor();
        }
    }

private:
    EditorTab* mTab = nullptr;
    QGraphicsScene* mScene = nullptr;
    QList<QGraphicsItem*> mOldSelection;
    QList<QGraphicsItem*> mNewSelection;
    bool mFirst = false;
};


class MoveItemsCommand : public QUndoCommand
{
public:
    MoveItemsCommand(QGraphicsScene* pScene, ItemPositionData oldPositions, ItemPositionData newPositions)
        : mScene(pScene),
        mOldPositions(oldPositions),
        mNewPositions(newPositions)
    {
        mFirst = true;

        if (mNewPositions.Count() == 1)
        {
            auto pNewLine = mNewPositions.FirstLinePos();
            auto pOldRect = mOldPositions.FirstRectPos();
            if (pNewLine)
            {
                auto pOldLine = mOldPositions.FirstLinePos();
                const bool posChange = pOldLine->x != pNewLine->x || pOldLine->y != pNewLine->y;
                const bool lineChanged = pOldLine->line != pNewLine->line;
                if (posChange && lineChanged)
                {
                    setText(QString("Move and resize collision"));
                }
                else if (posChange && !lineChanged)
                {
                    setText(QString("Move collision"));
                }
                else
                {
                    setText(QString("Move collision point"));
                }
            }
            else
            {
                auto pNewRect = mNewPositions.FirstRectPos();
                const bool posChange = pOldRect->x != pNewRect->x || pOldRect->y != pNewRect->y;
                const bool rectChanged = pNewRect->rect != pOldRect->rect;
                if (posChange && rectChanged)
                {
                    setText(QString("Move and resize map object"));
                }
                else if (posChange && !rectChanged)
                {
                    setText(QString("Move map object"));
                }
                else
                {
                    setText(QString("Resize map object"));
                }
            }
        }
        else
        {
            setText(QString("Move %1 item(s)").arg(mNewPositions.Count()));
        }
    }

    void redo() override
    {
        if (!mFirst)
        {
            mNewPositions.Restore();
            mScene->update();
        }
        mFirst = false;
    }

    void undo() override
    {
        mOldPositions.Restore();
        mScene->update();
    }

private:
    QGraphicsScene* mScene = nullptr;
    ItemPositionData mOldPositions;
    ItemPositionData mNewPositions;
    bool mFirst = false;
};

EditorTab::EditorTab(QWidget* aParent, UP_Model model)
    : QMainWindow(aParent),
    ui(new Ui::EditorTab),
    mModel(std::move(model))
{
    ui->setupUi(this);

    QGraphicsView* pView = ui->graphicsView;

    pView->setRenderHint(QPainter::SmoothPixmapTransform);
    pView->setRenderHint(QPainter::HighQualityAntialiasing);
    pView->setRenderHint(QPainter::Antialiasing);
    pView->setRenderHint(QPainter::TextAntialiasing);
    pView->setViewport(new QOpenGLWidget()); // Becomes owned by the view


    mScene = std::make_unique<EditorGraphicsScene>();

    connect(mScene.get(), &EditorGraphicsScene::SelectionChanged, this, [&](QList<QGraphicsItem*> oldSelection, QList<QGraphicsItem*> newSelection)
        {
            mUndoStack.push(new SetSelectionCommand(this, mScene.get(), oldSelection, newSelection));
        });

    connect(mScene.get(), &EditorGraphicsScene::ItemsMoved, this, [&](ItemPositionData oldPositions, ItemPositionData newPositions)
        {
            mUndoStack.push(new MoveItemsCommand(mScene.get(), oldPositions, newPositions));
        });

    iZoomLevel = 1.0f;
    for (int i = 0; i < 2; ++i)
    {
        //  ZoomIn();
    }

    setCentralWidget(ui->graphicsView);

    // Disable "already disabled" context menus on the QDockWidgets
    ui->propertyDockWidget->setContextMenuPolicy(Qt::PreventContextMenu);
    ui->undoHistoryDockWidget->setContextMenuPolicy(Qt::PreventContextMenu);

    const MapInfo& mapInfo = mModel->GetMapInfo();

    for (int x = 0; x < mapInfo.mXSize; x++)
    {
        for (int y = 0; y < mapInfo.mYSize; y++)
        {
            Camera* pCam = mModel->CameraAt(x, y);
            auto pCameraGraphicsItem = new CameraGraphicsItem(pCam, mapInfo.mXGridSize * x, y *  mapInfo.mYGridSize, mapInfo.mXGridSize, mapInfo.mYGridSize);
            mScene->addItem(pCameraGraphicsItem);

            if (pCam)
            {
                for (auto& mapObj : pCam->mMapObjects)
                {
                    auto pMapObject = new ResizeableRectItem(pView, mapObj.get());
                    mScene->addItem(pMapObject);
                }
            }
        }
    }

    for (auto& collision : mModel->CollisionItems())
    {
        auto pLine = new ResizeableArrowItem(pView, collision.get());
        mScene->addItem(pLine);
    }

    const int kXMargin = 100;
    const int kYMargin = 100;
    mScene->setSceneRect(-kXMargin, -kYMargin, (mapInfo.mXSize * mapInfo.mXGridSize) + (kXMargin * 2), (mapInfo.mYSize * mapInfo.mYGridSize) + (kYMargin * 2));


    ui->graphicsView->setScene(mScene.get());

    mUndoStack.setUndoLimit(100);
    ui->undoView->setStack(&mUndoStack);

    QTreeWidget* pTree = ui->treeWidget;

    // Two columns, property and value
    pTree->setColumnCount(2);

    // Set the header text
    QStringList headerStrings;
    headerStrings.append("Property");
    headerStrings.append("Value");
    pTree->setHeaderLabels(headerStrings);

    pTree->setAlternatingRowColors(true);
    pTree->setStyleSheet("QTreeView::item { height:23px; font:6px; padding:0px; margin:0px; }");
    
    pTree->header()->resizeSection(0, 200);
    pTree->header()->resizeSection(1, 90);

    pTree->setUniformRowHeights(true);

    pTree->setRootIsDecorated(false);


    addDockWidget(Qt::RightDockWidgetArea, ui->propertyDockWidget);
    addDockWidget(Qt::RightDockWidgetArea, ui->undoHistoryDockWidget);

    ui->propertyDockWidget->setMinimumWidth(310);

}

void EditorTab::ZoomIn()
{
    /*
    if (iZoomLevel < 1.0f + (KZoomFactor*KMaxZoomInLevels))
    {
        iZoomLevel += KZoomFactor;
        ui->graphicsView->scale(iZoomLevel, iZoomLevel);
    }
    */
}

void EditorTab::ZoomOut()
{
    /*
    if (iZoomLevel > 1.0f - (KZoomFactor*KMaxZoomOutLevels))
    {
        iZoomLevel -= KZoomFactor;
        ui->graphicsView->scale(iZoomLevel, iZoomLevel);
    }
    */
}

void EditorTab::ResetZoom()
{
    iZoomLevel = 1.0f;
    ui->graphicsView->scale(iZoomLevel, iZoomLevel);
}

EditorTab::~EditorTab()
{
    delete ui;
}

void EditorTab::ClearPropertyEditor()
{
    ui->treeWidget->clear();
}

void EditorTab::PopulatePropertyEditor(QGraphicsItem* pItem)
{
    ClearPropertyEditor();

    QTreeWidget* pTree = ui->treeWidget;

    auto pLine = qgraphicsitem_cast<ResizeableArrowItem*>(pItem);
    auto pRect = qgraphicsitem_cast<ResizeableRectItem*>(pItem);

    QList<QTreeWidgetItem*> items;
    QTreeWidgetItem* parent = nullptr;
    const QString kIndent("    ");
    if (pRect)
    {
        MapObject* pMapObject = pRect->GetMapObject();

        items.append(new QTreeWidgetItem(parent, QStringList({ kIndent + "Name", pMapObject->mName.c_str() })));

        items.append(new QTreeWidgetItem(parent, QStringList({ kIndent + "XPos", QString::number(pMapObject->mXPos) })));
        items.append(new QTreeWidgetItem(parent, QStringList({ kIndent + "YPos", QString::number(pMapObject->mYPos) })));
        items.append(new QTreeWidgetItem(parent, QStringList({ kIndent + "Width", QString::number(pMapObject->mWidth) })));
        items.append(new QTreeWidgetItem(parent, QStringList({ kIndent + "Height", QString::number(pMapObject->mHeight) })));

        for (UP_MapObjectProperty& property : pMapObject->mProperties)
        {
            if (property->mVisible)
            {
                QStringList strings;
                strings.append(kIndent + property->mName.c_str());

                const Model::FoundType foundType = mModel->FindType(property->mTypeName);
                if (foundType.mBasicType)
                {
                    strings.append(QString::number(property->mBasicTypeValue));
                }
                else if (foundType.mEnum)
                {
                    strings.append(property->mEnumValue.c_str());
                }
                items.append(new QTreeWidgetItem(parent, strings));
            }
        }
    }
    else if (pLine)
    {
        ICollision* pCollisionItem = pLine->GetCollisionItem();

        items.append(new QTreeWidgetItem(parent, QStringList({ kIndent + "X1", QString::number(pCollisionItem->mPos.mX1) })));
        items.append(new QTreeWidgetItem(parent, QStringList({ kIndent + "Y1", QString::number(pCollisionItem->mPos.mY1) })));
        items.append(new QTreeWidgetItem(parent, QStringList({ kIndent + "X2", QString::number(pCollisionItem->mPos.mX2) })));
        items.append(new QTreeWidgetItem(parent, QStringList({ kIndent + "Y2", QString::number(pCollisionItem->mPos.mY2) })));

        // TODO: Add AO/AE specific line props polymorphically
    }

    for (int i = 0; i < items.count(); i++)
    {
        const int b = (i % 2) == 0 ? 191 : 222;
        items[i]->setBackground(0, QColor(255, 255, b));
        items[i]->setBackground(1, QColor(255, 255, b));
    }

    pTree->insertTopLevelItems(0, items);

    /*
    auto spin = new QSpinBox();
    pTree->setItemWidget(items[2], 1, spin);
    */
}

void EditorTab::Undo()
{
    mUndoStack.undo();
}

void EditorTab::Redo()
{
    mUndoStack.redo();
}
