/*
  ==============================================================================

    Layout.cpp
    Created: 21 Feb 2016 9:14:52pm
    Author:  Daniel Walz

  ==============================================================================
*/

#include "Layout.h"


Layout::Layout(Orientation o, Component* owner)
  : orientation (o),
    isUpdating (false),
    isCummulatingStretch (false),
    owningComponent (owner)
{
}

Layout::~Layout()
{
}

void Layout::setOrientation (const Orientation o)
{
    orientation = o;
}


LayoutItem* Layout::addComponent (Component* c, int idx)
{
    LayoutItem* item = itemsList.insert (idx, new LayoutItem (c));
    updateGeometry();
    return item;
}

void Layout::removeComponent (Component* c)
{
    for (int i=0; i<itemsList.size(); ++i) {
        LayoutItem* item = itemsList.getUnchecked (i);
        if (item->isComponentItem() && item->getComponent() == c) {
            itemsList.remove (i);
        }
    }
    updateGeometry();
}

LayoutItem* Layout::addLabeledComponent (Component* c, Orientation o, Label** labelPtr, int idx)
{
    // if the layout is not owned by a component, the label will not show up,
    // because addAndMakeVisible can not be called.
    jassert (owningComponent);
    
    Label* label = new Label();
    if (owningComponent) {
        owningComponent->addAndMakeVisible (label);
    }
    Layout* sub = addSubLayout (o, idx, owningComponent);
    LayoutItem* labelItem = sub->addComponent (label);
    LabeledLayoutItem* labeledItem = new LabeledLayoutItem (c, label);
    sub->addRawItem (labeledItem);
    
    // set fixed height to font size
    float h = label->getFont().getHeight();
    labelItem->setMinimumHeight (h);
    labelItem->setMaximumHeight (h);

    if (labelPtr) {
        *labelPtr = label;
    }

    updateGeometry();
    return labeledItem;
}

LayoutItem* Layout::addLabeledComponent (Component* component, StringRef text, Orientation o, int idx)
{
    LayoutItem* item = addLabeledComponent(component, o, nullptr, idx);
    if (Label* label = item->getLabel()) {
        label->setText (text, dontSendNotification);
        label->setJustificationType (Justification::centred);
    }
    return item;
}

Layout* Layout::addSubLayout (Orientation o, int idx, Component* owner)
{
    SubLayout* sub = new SubLayout (o, owningComponent);
    itemsList.insert (idx, sub);
    updateGeometry();
    return sub;
}

LayoutItem* Layout::addSSpacer (float sx, float sy, int idx)
{
    LayoutItem* item = itemsList.insert (idx, new LayoutItem (LayoutItem::SpacerItem));
    updateGeometry();
    return item;
}

LayoutItem* Layout::getLayoutItem (Component* c)
{
    for (int i=0; i<itemsList.size(); ++i) {
        LayoutItem* item = itemsList.getUnchecked (i);
        if (item->isComponentItem() && item->getComponent() == c) {
            return item;
        }
        else if (item->isSubLayout()) {
            // search also sub layouts recoursively
            if (LayoutItem* subItem = getLayoutItem (c)) {
                return subItem;
            }
        }
    }
    return nullptr;
}

void Layout::addRawItem (LayoutItem* item, int idx)
{
    itemsList.insert (idx, item);
}


void Layout::updateGeometry ()
{
    if (owningComponent) {
        updateGeometry (owningComponent->getLocalBounds());
    }
}

void Layout::updateGeometry (Rectangle<int> bounds)
{
    // recursion check
    if (isUpdating) {
        return;
    }
    isUpdating = true;
    
    // remove items of deleted or invalid components
    for (int i=0; i<itemsList.size(); ++i) {
        LayoutItem* item = itemsList.getUnchecked (i);
        if (!item->isValid()) {
            itemsList.remove (i);
        }
    }

    itemsBounds.resize (itemsList.size ());

    float cummulatedX, cummulatedY;
    getCummulatedStretch (cummulatedX, cummulatedY);
    float availableWidth  = bounds.getWidth();
    float availableHeight = bounds.getHeight();
    
    if (orientation == TopDown || orientation == BottomUp) {
        for (int i=0; i<itemsList.size(); ++i) {
            LayoutItem* item = itemsList.getUnchecked (i);
            float sx, sy;
            item->getStretch (sx, sy);
            float h = bounds.getHeight() * sy / cummulatedY;
            Rectangle<int> childBounds (bounds.getX(), bounds.getY(), bounds.getWidth(), h);
            bool changedWidth, changedHeight;
            item->constrainBounds (childBounds, changedWidth, changedHeight);
            itemsBounds.set (i, childBounds);
            if (changedHeight) {
                itemBoundsFinal.set (i, true);
                availableHeight -= childBounds.getHeight();
                cummulatedY -= sy;
            }
            else {
                itemBoundsFinal.set (i, false);
            }
            if (changedWidth) {
                availableWidth = std::max (bounds.getWidth(), childBounds.getWidth());
            }
        }

        float y = bounds.getY();
        if (orientation == BottomUp) {
            y = bounds.getY() + bounds.getHeight();
        }
        for (int i=0; i<itemsList.size(); ++i) {
            LayoutItem* item = itemsList.getUnchecked (i);

            if (itemBoundsFinal.getUnchecked (i)) {
                float h = itemsBounds.getReference (i).getHeight();
                if (orientation == BottomUp) {
                    y -= h;
                }
                Rectangle<int> childBounds (bounds.getX(), y, availableWidth, h);
                if (item->isSubLayout()) {
                    if (Layout* sub = dynamic_cast<Layout*>(item)) {
                        sub->updateGeometry (childBounds);
                    }
                }
                if (Component* c = item->getComponent()) {
                    c->setBounds (childBounds);
                }

                if (orientation == TopDown) {
                    y += h;
                }
            }
            else {
                float sx, sy;
                item->getStretch (sx, sy);
                float h = availableHeight * sy /cummulatedY;
                if (orientation == BottomUp) {
                    y -= h;
                }
                Rectangle<int> childBounds (bounds.getX(), y, availableWidth, h );
                if (item->isSubLayout()) {
                    if (Layout* sub = dynamic_cast<Layout*>(item)) {
                        sub->updateGeometry (childBounds);
                    }
                }
                if (Component* c = item->getComponent()) {
                    c->setBounds (childBounds);
                }
                if (orientation == TopDown) {
                    y += h;
                }
            }
        }
    } else if (orientation == LeftToRight || orientation == RightToLeft) {
        for (int i=0; i<itemsList.size(); ++i) {
            LayoutItem* item = itemsList.getUnchecked (i);
            float sx, sy;
            item->getStretch (sx, sy);
            float w = bounds.getWidth() * sx / cummulatedX;
            Rectangle<int> childBounds (bounds.getX(), bounds.getY(), w, bounds.getHeight());
            bool changedWidth, changedHeight;
            item->constrainBounds (childBounds, changedWidth, changedHeight);
            itemsBounds.set (i, childBounds);
            if (changedWidth) {
                itemBoundsFinal.set (i, true);
                availableWidth -= childBounds.getWidth();
                cummulatedX -= sx;
            }
            else {
                itemBoundsFinal.set (i, false);
            }
            if (changedHeight) {
                availableHeight = std::max (bounds.getHeight(), childBounds.getHeight());
            }
        }

        float x = bounds.getX();
        if (orientation == RightToLeft) {
            x = bounds.getX() + bounds.getWidth();
        }
        for (int i=0; i<itemsList.size(); ++i) {
            LayoutItem* item = itemsList.getUnchecked (i);

            if (itemBoundsFinal.getUnchecked (i)) {
                float w = itemsBounds.getReference (i).getWidth();
                if (orientation == BottomUp) {
                    x -= w;
                }
                Rectangle<int> childBounds (x, bounds.getY(), w, availableHeight);
                if (item->isSubLayout()) {
                    if (Layout* sub = dynamic_cast<Layout*>(item)) {
                        sub->updateGeometry (childBounds);
                    }
                }
                if (Component* c = item->getComponent()) {
                    c->setBounds (childBounds);
                }

                if (orientation == TopDown) {
                    x += w;
                }
            }
            else {
                float sx, sy;
                item->getStretch (sx, sy);
                float w = availableWidth * sx /cummulatedX;
                if (orientation == RightToLeft) {
                    x -= w;
                }
                Rectangle<int> childBounds (x, bounds.getY(), w, availableHeight);
                if (item->isSubLayout()) {
                    if (Layout* sub = dynamic_cast<Layout*>(item)) {
                        sub->updateGeometry (childBounds);
                    }
                }
                if (Component* c = item->getComponent()) {
                    c->setBounds (childBounds);
                }
                if (orientation == LeftToRight) {
                    x += w;
                }
            }
        }
    }

    isUpdating = false;
}

void Layout::getCummulatedStretch (float& w, float& h) const
{
    w = 0.0;
    h = 0.0;
    
    if (isCummulatingStretch) {
        return;
    }
    isCummulatingStretch = true;
    
    for (int i=0; i<itemsList.size(); ++i) {
        LayoutItem* item = itemsList.getUnchecked (i);
        float x, y;
        item->getStretch (x, y);
        if (orientation == LeftToRight || orientation == RightToLeft) {
            w += x;
            h = std::max (h, y);
        }
        else if (orientation == TopDown || orientation == BottomUp) {
            w = std::max (w, x);
            h += y;
        }
        else {
            w += x;
            h += y;
        }
        ++item;
    }
    
    isCummulatingStretch = false;
}



// =============================================================================
SubLayout::SubLayout (Orientation o, Component* owner) : Layout (o, owner), LayoutItem (LayoutItem::SubLayout)
{
    
}


void SubLayout::getStretch (float& w, float& h) const
{
    w = 0.0;
    h = 0.0;
    getCummulatedStretch (w, h);
}

