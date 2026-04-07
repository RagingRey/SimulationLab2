#include <iostream>
#include "flatbuffers/flatbuffers.h"
#include "mydata_generated.h"

int main()
{
	// Create a FlatBufferBuilder
	flatbuffers::FlatBufferBuilder builder;

	// Create some data to serialize
	const auto pos = Lab700105::Vec3{ 1,2,3 };
	const auto colour = Lab700105::RGBA{ 1.0, 0.5, 0.5, 1.0 };
	const auto node = Lab700105::CreateNode(builder, 10, &pos, &colour);

	const auto posB = Lab700105::Vec3{ 4.0f, 5.0f, 6.0f };
	const auto colB = Lab700105::RGBA{ 0.2f, 0.7f, 1.0f, 1.0f };
	const auto nodeB = Lab700105::CreateNode(builder, 20, &posB, &colB);

	std::vector<flatbuffers::Offset<Lab700105::Node>> nodes;
	nodes.push_back(node);
	nodes.push_back(nodeB);

	const auto nodesOffset = builder.CreateVector(nodes);
	const auto root = Lab700105::CreateNodeList(builder, nodesOffset);

	builder.FinishSizePrefixed(root);

	// Get the pointer to the serialized data
	uint8_t* buf = builder.GetBufferPointer();
	const auto size = builder.GetSize();

	// Print the serialized data size
	std::cout << "Serialized data size: " << size << " bytes\n";

	const auto* data = flatbuffers::GetSizePrefixedRoot<Lab700105::NodeList>(buf);
	const auto* readNodes = data->nodes();

	if (readNodes != nullptr)
	{
		for (flatbuffers::uoffset_t i = 0; i < readNodes->size(); ++i)
		{
			const auto* n = readNodes->Get(i);
			std::cout << "Node[" << i << "] id: " << n->id() << '\n';

			if (const auto* p = n->position())
			{
				std::cout << "  position: (" << p->x() << ", " << p->y() << ", " << p->z() << ")\n";
			}

			if (const auto* c = n->colour())
			{
				std::cout << "  colour: (" << c->r() << ", " << c->g() << ", " << c->b() << ", " << c->a() << ")\n";
			}
		}
	}

	// Clean up and exit
	return 0;
}