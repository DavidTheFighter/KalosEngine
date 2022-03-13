#ifndef UTIL_SPACIALSTRUCTURES_H_
#define UTIL_SPACIALSTRUCTURES_H_

struct AABB
{
	svec4 aabbMin; // xyz - position, w - padding
	svec4 aabbMax; // xyz - position, w - padding
};

struct BoundingSphere
{
	svec3 position;
	float radius;
};

template <typename OctreePayload>
struct Octree
{
	Octree *parent;
	Octree *children[8]; // If a child == nullptr, then it doesn't have a child in that space
	AABB boundingBox;

	std::vector<OctreePayload> items;

	Octree()
	{
		parent = nullptr;
		children[0] = nullptr;
		children[1] = nullptr;
		children[2] = nullptr;
		children[3] = nullptr;
		children[4] = nullptr;
		children[5] = nullptr;
		children[6] = nullptr;
		children[7] = nullptr;
	}
};

inline bool AABBContainsSphere(const AABB &aabb, const BoundingSphere &sphere)
{
	return sphere.position.x - sphere.radius >= aabb.aabbMin.x && sphere.position.y - sphere.radius >= aabb.aabbMin.y && sphere.position.z - sphere.radius >= aabb.aabbMin.z &&
		sphere.position.x + sphere.radius <= aabb.aabbMax.x && sphere.position.y + sphere.radius <= aabb.aabbMax.y && sphere.position.z + sphere.radius <= aabb.aabbMax.z;
}

template <typename OctreePayload>
inline void insertItemsIntoOctree(Octree<OctreePayload> *node, const OctreePayload *itemsArray, size_t itemCount, float minOctreeLength = 0.1f)
{
	// Check to see if we can't subdivide any further, in which case just add all objects to this node
	if (node->boundingBox.aabbMax.x - node->boundingBox.aabbMin.x <= minOctreeLength)
	{
		node->items.insert(node->items.end(), itemsArray, itemsArray + itemCount);
		return;
	}

	float nodeAABBHalfLength = (node->boundingBox.aabbMax.x - node->boundingBox.aabbMin.x) * 0.5f;
	AABB childOctantBoxes[8];

	for (int x = 0; x < 2; x++)
		for (int y = 0; y < 2; y++)
			for (int z = 0; z < 2; z++)
				childOctantBoxes[z * 4 + y * 2 + x] = {{node->boundingBox.aabbMin.x + nodeAABBHalfLength * float(x), node->boundingBox.aabbMin.y + nodeAABBHalfLength * float(y), node->boundingBox.aabbMin.z + nodeAABBHalfLength * float(z)}, {node->boundingBox.aabbMin.x + nodeAABBHalfLength * float(x + 1), node->boundingBox.aabbMin.y + nodeAABBHalfLength * float(y + 1), node->boundingBox.aabbMin.z + nodeAABBHalfLength * float(z + 1)}};

	std::vector<OctreePayload> childNodeInsertions[8];

	for (size_t i = 0; i < itemCount; i++)
	{
		const OctreePayload &item = itemsArray[i];
		const BoundingSphere &itemBoundingSphere = item.getBoundingSphere();

		// Check to make sure this node can contain this item, if not then we should send this to it's parent (or if it doesn't have a parent just insert it anyway)
		if (AABBContainsSphere(node->boundingBox, itemBoundingSphere) || node->parent == nullptr)
		{
			// First check if a child node could better fit this item
			bool foundMoreOptimalChild = false;

			for (int child = 0; child < 8; child++)
			{
				if (AABBContainsSphere(childOctantBoxes[child], itemBoundingSphere))
				{
					foundMoreOptimalChild = true;
					if (node->children[child] == nullptr)
					{
						Octree<OctreePayload> *newChild = new Octree<OctreePayload>();
						newChild->parent = node;
						newChild->boundingBox = childOctantBoxes[child];
						node->children[child] = newChild;
					}

					childNodeInsertions[child].push_back(item);

					break;
				}
			}

			if (!foundMoreOptimalChild)
				node->items.push_back(item);
		}
		else
		{
			// If it doesn't fit in this node then send it up a level
			insertItemsIntoOctree(node->parent, &item, 1);
		}
	}

	for (int child = 0; child < 8; child++)
		if (childNodeInsertions[child].size() > 0)
			insertItemsIntoOctree(node->children[child], childNodeInsertions[child].data(), childNodeInsertions[child].size());
}

template <typename OctreePayload>
inline void insertItemsIntoOctree(Octree<OctreePayload> *node, const std::vector<OctreePayload> &items, float minOctreeLength = 0.1f)
{
	insertItemsIntoOctree(node, items.data(), items.size(), minOctreeLength);
}

template <typename OctreePayload>
inline void insertItemIntoOctree(Octree<OctreePayload> *node, const OctreePayload &item, float minOctreeLength = 0.1f)
{
	insertItemIntoOctree(node, &item, 1, minOctreeLength);
}

#endif /* UTIL_SPACIALSTRUCTURES_H_*/
