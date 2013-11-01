#ifndef _AOI_H
#define _AOI_H

#include "double_link.h"

#define BLOCK_WIDTH (10)        /*  单元格宽度       */
#define SCREEN_RADIUS (30)      /*  玩家视觉半径      */
#define MAP_WIDTH (400)         /*  地图宽度        */

#define RANGE_BLOCK_NUM ((SCREEN_RADIUS)/BLOCK_WIDTH)  /*   视觉半径内单元格数量  */

struct mapaoi_obj_s;

/*  AOI模块监听对象节点   */
struct map_aoiobj_link_node_s
{
    struct double_link_node_s  node;
    struct mapaoi_obj_s* obj;
};

class MapAoi;

/*  地图单元格   */
class MapAoiBlock
{
public:
    /*  检测坐标是否属于此单元格  */
    bool    contain(int x, int y);
    int     getRow();
    int     getColumn();

    /*  获取通知列表 */
    struct double_link_s*   getObserverList();

private:
    friend class MapAoi;

    void    setPos(int x, int y);
    void    setRowColumn(int row, int column);

private:
    /*  观察者(通知)列表   */
    struct double_link_s    m_observer_list;
    /*  单元格左上角坐标    */
    int                     m_x,  m_y;
    /*  此单元格在地图的单元格数组中的行列号  */
    int                     m_row;
    int                     m_column;
};

#define PLAYER_LINK_NODE_SIZE (2*RANGE_BLOCK_NUM+1)

/*  AOI模块管理的基本对象    */
struct mapaoi_obj_s
{
public:
    mapaoi_obj_s();

    int             getX() const;
    int             getY() const;

    MapAoiBlock*    getMapBlock();

protected:
    void            setXY(int x, int y);

private:
    friend class MapAoi;
    void            setMapBlock(MapAoiBlock* block);

private:
    int             m_x;
    int             m_y;

    /*  所属单元格 */
    MapAoiBlock*    m_block;

    /*  每个玩家的注册区域:用于将玩家添加到某单元格的观察者列表所使用的node   */
    struct map_aoiobj_link_node_s   m_nodes[PLAYER_LINK_NODE_SIZE][PLAYER_LINK_NODE_SIZE];
};

/*  地图AOI模块 */
class MapAoi
{
public:
    MapAoi();
    ~MapAoi();

    /*  注册或取消注册到玩家所属位置单元格以及它周围单元格(满足视觉大小):返回值为玩家当前位置所属单元格    */
    MapAoiBlock*    registerBlock(struct mapaoi_obj_s* player, bool b_enter);
    bool            isValid(int x, int y);
private:
    int             getBlockIndex(int pos);
    /*  根据坐标获取对应的单元格    */
    MapAoiBlock*    getBlock(int x, int y);

private:
    MapAoiBlock*    m_blocks;
    int             m_side_block_num;   /*  边长包含的单元格数量  */
};

#endif