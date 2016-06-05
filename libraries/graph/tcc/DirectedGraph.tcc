//
// DirectedGraph.tcc
//

namespace DirectedGraphImpl
{
    // TODO: put this in some utility place
    template <typename ContainerType>
    class ReverseRange
    {
    public:
        ReverseRange(const ContainerType& container) : _begin(container.crbegin()), _end(container.crend()) {}
        typename ContainerType::const_reverse_iterator begin() const { return _begin; }
        typename ContainerType::const_reverse_iterator end() const { return _end; }
    
    private:
        typename ContainerType::const_reverse_iterator _begin;
        typename ContainerType::const_reverse_iterator _end;    
    };
    
    template <typename ContainerType>
    ReverseRange<ContainerType> Reverse(const ContainerType& container)
    {
        return ReverseRange<ContainerType>(container);
    }
}

//
// Factory method
//
template <typename NodeType, typename... Args>
std::shared_ptr<NodeType> DirectedGraph::AddNode(Args... args)
{
    // TODO: Store node somewhere
    auto result = std::make_shared<NodeType>(args...);
    _nodeMap[result->Id()] = result;
    result->AddDependents();
    return result;
}

//
// Compute output value
//
template <typename ValueType>
std::vector<ValueType> DirectedGraph::GetNodeOutput(const std::shared_ptr<Node>& outputNode, size_t outputIndex) const
{
    auto compute = [](const Node& node)
    {
        node.ComputeOutput();
    };
    
    Visit(compute, outputNode);

    return outputNode->GetOutputValue<ValueType>(outputIndex);
}

template <typename ValueType>
std::vector<ValueType> DirectedGraph::GetNodeOutput(const NodeOutput<ValueType>& nodeOutput) const
{
    auto compute = [](const Node& node)
    {
        node.ComputeOutput();
    };

    // TODO: make version of Visit that doesn't require a shared_ptr
    Visit(compute, {nodeOutput.GetNode()});

    return nodeOutput.GetOutput();
}

//
// Visitors
//

// Visits the entire graph
// TODO: merge code for this and active-graph visitor
template <typename Visitor>
void DirectedGraph::Visit(Visitor& visitor) const
{
    std::vector<const Node*> emptyVec;
    Visit(visitor, emptyVec);
}


// Visits just the parts necessary to compute output
template <typename Visitor>
void DirectedGraph::Visit(Visitor& visitor, const std::shared_ptr<Node>& outputNode) const
{
    std::vector<std::shared_ptr<Node>> x = {outputNode};
    Visit(visitor, {outputNode.get()});
}

// Visits just the parts necessary to compute outputs
template <typename Visitor>
void DirectedGraph::Visit(Visitor& visitor, const std::vector<std::shared_ptr<Node>>& outputNodes) const
{
    // start with output nodes in the stack
    std::vector<const Node*> outputNodePtrs;
    for(auto outputNode: outputNodes)
    {
        outputNodePtrs.push_back(outputNode.get());
    }
    Visit(visitor, outputNodePtrs);
}

// TODO: merge implementations into this function --- use "visit full graph" semantics with outputNodePtrs is empty
template <typename Visitor>
void DirectedGraph::Visit(Visitor& visitor, const std::vector<const Node*>& outputNodePtrs) const
{
    if(_nodeMap.size() == 0)
    {
        return;
    }
        
    // start with output nodes in the stack
    std::unordered_set<const Node*> visitedNodes;
    std::vector<const Node*> stack = outputNodePtrs;
    
    const Node* sentinelNode = nullptr;
    if(stack.size() == 0) // Visit full graph
    {
        // helper function to find a terminal node
        auto IsLeaf = [](const Node* node) { return node->GetDependents().size() == 0; };
        // start with some arbitrary node
        const Node* anOutputNode = _nodeMap.begin()->second.get();
        // follow dependency chain until we get an output node
        while(!IsLeaf(anOutputNode))
        {
            anOutputNode = anOutputNode->GetDependents()[0];
        } 
        stack.push_back(anOutputNode);
        sentinelNode = anOutputNode;
    }
    
    while (stack.size() > 0)
    {
        const Node* node = stack.back();

        // check if we've already visited this node
        if (visitedNodes.find(node) != visitedNodes.end())
        {
            stack.pop_back();
            continue;
        }

        // we can visit this node only if all its inputs have been visited already
        bool canVisit = true;
        for (auto input: node->_inputs)
        {
            // Note: If NodeInputs can point to multiple nodes, we'll have to iterate over them here
            auto inputNode = input->GetNode(); 
            canVisit = canVisit && visitedNodes.find(inputNode) != visitedNodes.end();
        }

        if (canVisit)
        {
            stack.pop_back();
            visitedNodes.insert(node);
    
            // In "visit whole graph" mode, we want to defer visiting the chosen output node until the end
            // In "visit active graph" mode, this test should never fail, and we'll always visit the node        
            if(node != sentinelNode)
            {
                visitor(*node);
            }
            
            if(sentinelNode != nullptr) // sentinelNode is non-null only if we're in visit-whole-graph mode
            {
                // now add all our children (Note: this part is the only difference between visit-all and visit-active-graph
                for(const auto& child: DirectedGraphImpl::Reverse(node->_dependentNodes)) // Visiting the children in reverse order more closely retains the order the features were originally created
                {
                    // note: this is kind of inefficient --- we're going to push multiple copies of child on the stack. But we'll check if we've visited it already when we pop it off. 
                    // TODO: optimize this if it's a problem
                    stack.push_back(child);
                }
            }
        }
        else // visit node's inputs
        {
            for (auto input: DirectedGraphImpl::Reverse(node->_inputs)) // Visiting the inputs in reverse order more closely retains the order the features were originally created
            {
                stack.push_back(input->GetNode()); // Again, if `NodeInput`s point to multiple nodes, need to iterate here
            }
        }
    }    
    if(sentinelNode != nullptr)
    {
        visitor(*sentinelNode);
    }
}

