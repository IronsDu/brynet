#ifndef _AOI_H
#define _AOI_H

#define BLOCK_WIDTH (10)		/*	单元格宽度	*/
#define SCREEN_RADIUS (10)		/*	玩家视觉范围	*/
#define MAP_WIDTH (800)			/*	地图宽度		*/

#define RANGE_BLOCK_NUM ((SCREEN_RADIUS)/BLOCK_WIDTH)  /*   BLOCK数量  */

struct mapaoi_obj_s;

struct map_aoiobj_link_node_s
{
	struct double_link_node_s  node;
	struct mapaoi_obj_s* obj;
};

class MapAoi;

class MapAoiBlock
{
public:
	bool    contain(int x, int y);
	int     getRow();
	int     getColumn();

	struct double_link_s*   getObserverList();

private:
	friend class MapAoi;

	void	setPos(int x, int y);
	void    setRowColumn(int row, int column);

private:
	/*  观察者(通知)列表   */
	struct double_link_s    m_observer_list;
	/*	block左上角坐标	*/
	int                     m_x,  m_y;
	/*	block在地图的block数组中的行列号	*/
	int                     m_row;
	int                     m_column;
};

#define PLAYER_LINK_NODE_SIZE (2*RANGE_BLOCK_NUM+1)

struct mapaoi_obj_s
{
	char            name[10];

public:
	mapaoi_obj_s();

	int			    getX();
	int			    getY();

	MapAoiBlock*    getMapBlock();

	void		    setXY(int x, int y);

	/*	TODO::提取到派生类	*/
	bool		    isNotifyed();
	void		    setNotifyed(bool notifyed);

private:
	friend class MapAoi;
	void		    setMapBlock(MapAoiBlock* block);

private:
	int			    m_x;
	int			    m_y;
	bool		    m_notifyed;	/*	是否被通知	*/

    /*  所属block */
    MapAoiBlock*    m_block;

    /*  每个玩家的注册区域:用于将玩家添加到某BLOCK的观察者列表所使用的node   */
    struct map_aoiobj_link_node_s   m_nodes[PLAYER_LINK_NODE_SIZE][PLAYER_LINK_NODE_SIZE];
};

class MapAoi
{
public:
	MapAoi();
	
	/*  注册或取消注册到玩家所属位置BLOCK以及它周围BLOCKS(满足视觉大小):返回值为玩家当前位置所属block    */
	MapAoiBlock*	registerBlock(struct mapaoi_obj_s* player, bool b_enter);
private:
	int			    getBlockIndex(int pos);
	/*  根据坐标获取对应的BLOCK  */
	MapAoiBlock*    getBlock(int x, int y);

private:
	MapAoiBlock*    m_blocks;
	int             m_side_block_num;   /*  边长包含的block数量    */
};

#endif