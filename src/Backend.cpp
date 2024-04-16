//qt headers
#include <QDebug>
#include <QPushButton>
#include <QBoxLayout>
#include <QTextEdit>
#include <QLabel>

//cpp headers
#include <iostream>
#include <vector>
#include <stack>
#include <map>
#include <random>
#include <string>
#include <new>
#include <cctype>
#include <sstream>

//include headers
#include "Backend.h"
#include "../RBTree/RBTree.h"

//c headers
#include <unistd.h>


void Backend::errorHandle(int ERROR_CODE) {
    switch (ERROR_CODE) {
        case 9: {
            qDebug() << std::string("ERROR_CODE is " + std::to_string(ERROR_CODE) + " -> bad allocation memory\n").data();
            break;
        }
    }
}

static inline size_t unwrap_or(const void *const ptr, size_t ofst, size_t default_value) {
    return (ptr ? (*(size_t*)(((char*)ptr) + ofst)) : default_value);
}

void printStateNodes(StateForWidthCalc *stateFWCalc) {
    StackNode *p{0};
    if (!stateFWCalc) {
        return;
    }
    p = stateFWCalc->top;
    qDebug() << "________________________________________";
    while (p) {
        qDebug() << "RBNode_t's value: " << p->nodePtr->value;
        qDebug() << "StackNode's width position: " << p->width;
        qDebug() << "StackNode's Y position: " << p->y;
        p = p->previous;
    }
    qDebug() << "________________________________________";
}

void freeStackNodes(StateForWidthCalc *state) {
    StackNode *p = NULL;
    StackNode *tmp = NULL;
    if (!state || !state->top) return;
    p = state->top;
    while (p) {
       tmp = p->previous;
       free(p);
       p = tmp;
    }
    state->top = NULL;
}

void calcWidthX(const RBNode_t *node, void* state) {
    StateForWidthCalc *st = (StateForWidthCalc*)state;
    if (!st) return;
    SN_push(&(st->top), node);
    size_t ofst = offsetof(StackNode, width);
    st->top->width = 2*st->r+2*std::max(unwrap_or(SN_find(st->top, node->left), ofst, st->minWidth), unwrap_or(SN_find(st->top, node->right), ofst, st->minWidth));

    st->top->y = *(st->rdata.y_lvl_cur) + 1;
}

size_t calcHeightY(const RBNode_t *nodeToAdd) {
    size_t yHeight{0};
    const RBNode_t *tmp = nodeToAdd;
    if (!tmp) {
        return -1;
    }
    while (tmp->parent) {
        tmp = tmp->parent;
        yHeight++;
    }
    return yHeight;
}

bool checkConsistencyOfTheTree(const RBNode_t *root) {
    if (!root) return true;

    if (root->parent) {
        qDebug() << "ERROR: root has parent: root->value: " << root->value << ", root->parent->value: " << root->parent->value;
        return false;
    }

    std::stack<const RBNode_t *> v;
    std::map<const RBNode_t *, int> m;

    v.push(root);
    m[root] = 0;

    while (!v.empty()) {
        const RBNode_t *cur = v.top();
        if (cur->left) {
            if (!m[cur]) {
                if (cur->left->parent != cur) {
                    qDebug() << "ERROR: " << __PRETTY_FUNCTION__ << ": v.size() = " << v.size() << ": left has wrong parent";
                    qDebug() << "ERROR: " << "current value is " << cur->value << ", left value is " << cur->left->value;
                    qDebug() << "ERROR: " << "current == current->left: " << (cur == cur->left);
                    if (cur->left->parent) {
                        qDebug() << "cur->left->parent there is";
                    } else {
                        qDebug() << "ERROR: " << "left has no parent";
                    }
                    return false;
                }
                v.push(cur->left);
                m[cur] = 1;
                continue;
            }
        } else if (!m[cur]){
            m[cur] = 1;
        }
        if (cur->right) {
            if (m[cur] == 1) {
                if (cur->right->parent != cur) {
                    qDebug() << "ERROR: " << __PRETTY_FUNCTION__ << ": v.size() = " << v.size() << ": right has wrong parent";
                    qDebug() << "ERROR: " << "current value is " << cur->value << ", right value is " << cur->right->value;
                    qDebug() << "ERROR: " << "{current, current->right, current->right->par}\n" << "{" << cur << ", " << cur->right << ", " << cur->right->parent << "}";
                    if (cur->right->parent) {
                        qDebug() << "ERROR: " << "right has parent, its value: " << cur->right->parent->value;
                    } else {
                        qDebug() << "ERROR: " << "right has no parent";
                    }
                    return false;
                }
                v.push(cur->right);
                m[cur] = 2;
                continue;
            }
        } else if (m[cur] == 1) {
            m[cur] = 2;
        }
        if ((!cur->left && !cur->right) || m[cur] == 2) {
            v.pop();
        }
    }

    return true;
}

void runOverTheTree(RBNode_t *root, void(*func)(const RBNode_t *Node, void* state), void* state) {
    static size_t y_lvl = 0;
    if (!root) {
        return;
    }
    RunOverTheTreeData *st = (RunOverTheTreeData*)(state);
    if (!y_lvl && st) {st->y_lvl_cur = &y_lvl;}

    y_lvl++;
    runOverTheTree(root->left, func, state);
    runOverTheTree(root->right, func, state);
    y_lvl--;
    func(root, state);
}

const StackNode* SN_find(const StackNode *top, const RBNode_t *node) {
    const StackNode *p = top;
    while (p && p->nodePtr != node) {
        p = p->previous;
    }
    return p;
}

StackNode* SN_find_rw(StackNode *top, const RBNode_t *node) {
    StackNode *p = top;
    while (p && p->nodePtr != node) p = p->previous;
    return p;
}

void SN_push(StackNode **Node, const RBNode_t *value) {
    if (!Node) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": Invalid StackNode");
    }
    if (!*Node) {
        *Node = new StackNode;
        (*Node)->nodePtr = value;
        (*Node)->previous = nullptr;
        (*Node)->width = 0; //минимальная ширина
    } else {
        StackNode *newNode = new StackNode;
        newNode->nodePtr = value;
        newNode->previous = *Node;
        newNode->width = 0;
        *Node = newNode;
    }
}

void SN_pop(StackNode **Node) {
    if (!Node) throw std::runtime_error(std::string(__FUNCTION__) + ": Invalid StackNode");
    if (!*Node) return;
    StackNode *nodeToDelete = *Node;
    *Node = (*Node)->previous;
    delete nodeToDelete;
}

void pushFront(ListNode **Node, RBNode_t *value) {
    if (!Node) {
        throw std::runtime_error("Node is invalid\n");
    }
    if (!*Node) {
        *Node = new ListNode;
        (*Node)->right = nullptr;
        (*Node)->left = nullptr;
        (*Node)->value = value;
        (*Node)->index = 0;
    } else {
        ListNode *newNode = new ListNode;
        newNode->right = *Node;
        newNode->left = nullptr;
        newNode->value = value;
        newNode->index = (*Node)->index + 1;
        (*Node)->left = newNode;
        *Node = newNode;
    }
}

const RBNode_t *popBack(ListNode **Node) {
    if (!Node || !*Node) return nullptr;
    ListNode *nodeToDelete = *Node;
    while (nodeToDelete->right) nodeToDelete = nodeToDelete->right;
    const RBNode_t *delValue = nodeToDelete->value;
    if (nodeToDelete->left) nodeToDelete->left->right = nullptr;
    else *Node = nullptr;
    delete nodeToDelete;
    return delValue;
}

void printList(const ListNode *Node) {
    const ListNode *p = Node;
    if (!Node) return;
    while (p) {
        qDebug() << "RBNode value is " << p->value->value;
        p = p->right;
    }
}

void runWidthTheTree(RBNode_t *root, void(*func)(const RBNode_t *Node, void *state), void *state) {
    ListNode *Node{nullptr};
    try {
        pushFront(&Node, root);
        while (Node) {
            const RBNode_t *rbNode = popBack(&Node);
            func(rbNode, state);
            if (rbNode->left) pushFront(&Node, rbNode->left);
            if (rbNode->right) pushFront(&Node, rbNode->right);
        }
    } catch(const std::exception &ex) {
       qDebug() << ex.what();
    }
}

void calcShiftX(const RBNode_t *root, void *state) {
    StateForWidthCalc *st = static_cast<StateForWidthCalc*>(state);
    StackNode *p = st->top;
    StackNode *snParent{0};
    const RBNode_t *rbNode = root;
    if (!root || !st) {
        throw;
    }
    p = SN_find_rw(st->top, rbNode);
    if (!p->nodePtr->parent) { st->top->x = 0; }
    else {
        snParent = SN_find_rw(st->top, rbNode->parent);
        if (snParent->nodePtr->right == rbNode) {
            p->x = snParent->x + snParent->width;
        } else {
            p->x = snParent->x - snParent->width;
        }
    }
}

Backend::Backend(QApplication &_app, QWidget *parent): QWidget(parent), app{_app} {
    trW = new TreeWidth;
    stateFWCalc.minWidth = 10;

    QWidget *wgt = new QWidget(this);
    wgt->setFixedWidth(200);

    QGridLayout *gd = new QGridLayout(wgt);

    //Hide/Show GUI_______________________________
    QPushButton *buttonHide = new QPushButton("HideGUI");
    QPushButton *buttonShow = new QPushButton("ShowGUI", this);
    buttonShow->hide();
    buttonShow->setFixedWidth(190);
    QObject::connect(buttonHide, &QPushButton::clicked, this, [w = wgt, buttonShow]{w->hide(); buttonShow->show();});
    QObject::connect(buttonShow, &QPushButton::clicked, this, [wgt, buttonShow]{buttonShow->hide(); wgt->show();});

    gd->addWidget(buttonHide, 0, 0, 1, 2);
    buttonShow->hide();
    buttonHide->hide();
    //Hide/Show GUI_______________________________

    //HalfX/DoubleX_______________________
    QHBoxLayout *hlayoutModX = new QHBoxLayout();

    QPushButton *buttonHalfX = new QPushButton("Half X");
    QObject::connect(buttonHalfX, SIGNAL(clicked()), this, SLOT(halfFactorX()));

    QPushButton *buttonDoubleX = new QPushButton("Double X");
    QObject::connect(buttonDoubleX, SIGNAL(clicked()), this, SLOT(doubleFactorX()));

    hlayoutModX->addWidget(buttonDoubleX);
    hlayoutModX->addWidget(buttonHalfX);
    gd->addLayout(hlayoutModX, 2, 0, 1, 2);
    //HalfX/DoubleX_______________________

    //Input range/number to add/delete
    qte = new QTextEdit();
    qte->setPlaceholderText(QStringLiteral("Input number or range via ','"));
    qte->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    qte->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    qte->setFixedHeight(30);

    gd->addWidget(qte, 3, 0, 1, 2);
    //Input range/number to add/delete

    //Additing/Deleting Value buttons
    QPushButton *buttonAddValue = new QPushButton("Add Value");
    QObject::connect(buttonAddValue, SIGNAL(clicked()), this, SLOT(slotAddValue()));

    QPushButton *buttonDeleteValue = new QPushButton("Delete Value");
    QObject::connect(buttonDeleteValue, SIGNAL(clicked()), this, SLOT(slotDeleteValue()));

    gd->addWidget(buttonAddValue, 4, 0, 1, 1);
    gd->addWidget(buttonDeleteValue, 4, 1, 1, 1);
    //Additing/Deleting Value buttons

    //Delete/Save Tree buttons
    QPushButton *buttonDeleteTree = new QPushButton("Clear All");
    QObject::connect(buttonDeleteTree, SIGNAL(clicked()), this, SLOT(slotDeleteTree()));
    gd->addWidget(buttonDeleteTree, 6, 0, 1, 1);

    //Delete/Save Tree buttons
}

Backend::~Backend() {
    free(rbroot);
}

void Backend::halfFactorX() {
    factorX /= 1.5;
    treeChanged = true;
    repaint();
}

void Backend::doubleFactorX() {
    factorX *= 1.5;
    treeChanged = true;
    repaint();
}

void Backend::paintEvent(QPaintEvent *ev) {
    Q_UNUSED(ev);

    QPainter *QPainterPtr = new QPainter(this);
    QPainterPtr->setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::FlatCap));
    QPainterPtr->eraseRect(0, 0, width(), height());
    if (rbroot) {
        stateFWCalc.r = 50*factor;
        if (treeChanged) {
            freeStackNodes(&stateFWCalc);
            runOverTheTree(rbroot, calcWidthX, &stateFWCalc);
            runWidthTheTree(rbroot, calcShiftX, &stateFWCalc);
        }

        StackNode *tmp = stateFWCalc.top;
        StackNode *tmpTop = tmp;
        StackNode *tmpParent;

        size_t addWidth = width()/2;
        size_t addHeight = 20;
        ssize_t heightFactor = 50;

        while (tmp) {
            QPainterPtr->setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::FlatCap));
            if (nodeColor(tmp->nodePtr)) QPainterPtr->setBrush(QBrush(Qt::red, Qt::SolidPattern));
            else QPainterPtr->setBrush(QBrush(Qt::black, Qt::SolidPattern));
            tmpParent = SN_find_rw(tmpTop, tmp->nodePtr->parent);
            if (tmpParent) {
                ssize_t x1 = (ssize_t)tmp->x * factorX + addWidth + stateFWCalc.r/2;
                ssize_t y1 = (ssize_t)tmp->y * heightFactor + addHeight;
                ssize_t x2 = (ssize_t)tmpParent->x * factorX + addWidth + stateFWCalc.r/2;
                ssize_t y2 = (ssize_t)tmpParent->y * heightFactor + addHeight + stateFWCalc.r;
                QPainterPtr->drawLine(x1, y1, x2, y2);
            }

            QPainterPtr->drawEllipse((ssize_t)tmp->x * factorX + addWidth, tmp->y * heightFactor + addHeight, stateFWCalc.r, stateFWCalc.r);
            QPainterPtr->drawText((ssize_t)tmp->x * factorX + addWidth + stateFWCalc.r/2.5, tmp->y * heightFactor + stateFWCalc.r/2 + addHeight, QString::number(tmp->nodePtr->value));
            tmp = tmp->previous;
        }
    }
    QPainterPtr->end();
    treeChanged = false;
}

void Backend::slotAddValue() {
    if (qte->toPlainText().trimmed().isEmpty()) return;

    std::string inputStr = qte->toPlainText().trimmed().toUtf8().constData();
    for (size_t i = 0; i < inputStr.length(); i++) {
        if (!std::isdigit(inputStr[i])) {
            return;
        }
    }
    tmpValue = qte->toPlainText().trimmed().toLongLong();
    RBNode_t *tmp = addValue(&rbroot, tmpValue, &ERROR_CODE);

    if (ERROR_CODE) {
        qDebug() << "function addValue returned ERROR_CODE " << ERROR_CODE;
        qDebug() << "the value was " << tmpValue;
        qDebug() << "ERROR_CODE will be changed to 0";
        ERROR_CODE = 0;
    }

    if (tmp) { 
        treeChanged = true;
        qDebug() << "treeChanged is set to true";
        if (checkConsistencyOfTheTree(rbroot)) {
            qDebug() << "checkConsistencyOfTheTree(rbroot): " << checkConsistencyOfTheTree(rbroot);
        }
    } else {
        qDebug() << "addValue returned null";
    }
    repaint();
}


void Backend::slotDeleteValue() {
    if (qte->toPlainText().trimmed().isEmpty()) {
        return;
    }
    std::string inputStr = qte->toPlainText().trimmed().toUtf8().constData();
    for (size_t i = 0; i < inputStr.length(); i++) {
        if (!std::isdigit(inputStr[i])) {
            return;
        }
    }
    tmpValue = qte->toPlainText().trimmed().toLongLong();
    deleteNode(&rbroot, tmpValue, &ERROR_CODE);
    if (ERROR_CODE) {
        if (ERROR_CODE == 3 || ERROR_CODE == 6) {
            qDebug() << "you'are trying to delete a nonexistent node";
        }
        qDebug() << "function deleteNode returned ERROR_CODE " << ERROR_CODE;
        qDebug() << "the value was " << tmpValue;
        qDebug() << "ERROR_CODE will be changed to 0";
        ERROR_CODE = 0;
    }
    treeChanged = true;
    repaint();
}

void Backend::slotDeleteTree() {
    deleteTree(rbroot);
    rbroot = NULL;
    treeChanged = true;
    repaint();
}

void Backend::resizeEvent(QResizeEvent *ev) {
    Q_UNUSED(ev)
}
