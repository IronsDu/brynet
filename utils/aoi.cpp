#include <stdlib.h>
#include <stdio.h>

#include "aoi.h"

bool MapAoiBlock::contain(int x, int y)
{
    return (x >= m_x && x < (m_x+BLOCK_WIDTH) && y >= m_y && y < (m_y+BLOCK_WIDTH));
}

void MapAoiBlock::setPos(int x, int y)
{
    m_x = x;
    m_y = y;
}

void MapAoiBlock::setRowColumn(int row, int column)
{
    m_row = row;
    m_column = column;
}

int MapAoiBlock::getRow()
{
    return m_row;
}

int MapAoiBlock::getColumn()
{
    return m_column;
}

struct double_link_s* MapAoiBlock::getObserverList()
{
    return &m_observer_list;
}

mapaoi_obj_s::mapaoi_obj_s()
{
    for(int i = 0; i < PLAYER_LINK_NODE_SIZE; ++i)
    {
        for(int j = 0; j < PLAYER_LINK_NODE_SIZE; ++j)
        {
            m_nodes[i][j].obj = this;
            m_nodes[i][j].node.next = NULL;
            m_nodes[i][j].node.prior = NULL;
        }
    }

    m_block = NULL;
}

int mapaoi_obj_s::getX() const
{   
    return m_x; 
}

int mapaoi_obj_s::getY() const
{   
    return m_y; 
}

MapAoiBlock* mapaoi_obj_s::getMapBlock()   
{   
    return  m_block;    
}

void mapaoi_obj_s::setMapBlock(MapAoiBlock* block)
{
    m_block = block;
}

void mapaoi_obj_s::setXY(int x, int y) 
{   
    m_x = x; 
    m_y = y;   
}

MapAoi::MapAoi()
{
    int times = MAP_WIDTH / BLOCK_WIDTH;
    if(MAP_WIDTH % BLOCK_WIDTH) times += 1;

    m_blocks = new MapAoiBlock[times*times];

    int row = 0;

    for(int row = 0; row < times; ++row)
    {
        for(int column = 0; column < times; ++column)
        {
            int index = row*times + column;

            m_blocks[index].setPos(row * BLOCK_WIDTH, column * BLOCK_WIDTH);
            m_blocks[index].setRowColumn(row, column);

            double_link_init(m_blocks[index].getObserverList());
        }
    }

    m_side_block_num = times;
}

MapAoi::~MapAoi()
{
    if(m_blocks != NULL)
    {
        delete [] m_blocks;
        m_blocks = NULL;
    }
}

int MapAoi::getBlockIndex(int pos)
{
    return pos / BLOCK_WIDTH;
}

bool MapAoi::isValid(int x, int y)
{
    return getBlock(x, y) != NULL;
}

MapAoiBlock* MapAoi::getBlock(int x, int y)
{
    MapAoiBlock* ret = NULL;
    int row = getBlockIndex(x);
    int column = getBlockIndex(y);

    if(row >= 0 && row < m_side_block_num && column >= 0 && column < m_side_block_num)
    {
        ret = m_blocks+row*m_side_block_num+column;
    }

    return ret;
}

MapAoiBlock* MapAoi::registerBlock(struct mapaoi_obj_s* player, bool b_enter)
{
    MapAoiBlock* ret = getBlock(player->getX(), player->getY());

    if(ret == NULL)
    {
        return NULL;
    }

    if(b_enter && player->getMapBlock() == ret)
    {
        return NULL;
    }

    if(!b_enter && player->getMapBlock() != ret)
    {
        return NULL;
    }

    int row = ret->getRow();
    int column = ret->getColumn();

    int startRow = row - RANGE_BLOCK_NUM;
    if(startRow < 0)    startRow = 0;
    int endRow = row + RANGE_BLOCK_NUM;
    if(endRow >= m_side_block_num) endRow = m_side_block_num-1;

    int startColumn = column - RANGE_BLOCK_NUM;
    if(startColumn < 0) startColumn = 0;
    int endColumn = column + RANGE_BLOCK_NUM;
    if(endColumn >= m_side_block_num)  endColumn = m_side_block_num-1;

    struct map_aoiobj_link_node_s   (&nodes)[PLAYER_LINK_NODE_SIZE][PLAYER_LINK_NODE_SIZE] = player->m_nodes;
    for(int i = startRow; i <= endRow; ++i)
    {
        for(int j = startColumn; j <= endColumn; ++j)
        {
            if(b_enter)
            {
                double_link_push_back(m_blocks[i*m_side_block_num+j].getObserverList(), &nodes[i-startRow][j-startColumn].node);
            }
            else
            {
                double_link_erase(m_blocks[i*m_side_block_num+j].getObserverList(), &nodes[i-startRow][j-startColumn].node);
                nodes[i-startRow][j-startColumn].node.next = nodes[i-startRow][j-startColumn].node.prior = NULL;
            }
        }
    }

    if(b_enter)
    {
        player->setMapBlock(ret);
    }
    else
    {
        player->setMapBlock(NULL);
    }

    return ret;
}