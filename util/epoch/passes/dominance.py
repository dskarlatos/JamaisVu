import logging
import networkx as nx

from sir import Function, ASAError


class NoEntryForDomTreeError(ASAError): pass


class DominancePass:
    def __init__(self, function: Function):
        self.function = function

    def run(self):
        dom1 = self._build_dom_sets(self.function.cfg, 'cfg')
        dom2 = self._build_dom_sets(self.function.cfg.reverse(), 'rcfg')
        self.function._dom_tree, self.function._dominances, self.function._dom_frontiers = dom1
        self.function._p_dom_tree, self.function._p_dominances, self.function._p_dom_frontiers = dom2

        try:
            for bb in self.function:
                assert(bb in self.function._dom_tree.nodes)
                assert(bb in self.function._p_dom_tree.nodes)
                assert(bb in self.function._dominances)
                assert(bb in self.function._p_dominances)
                assert(bb in self.function._dom_frontiers)
                assert(bb in self.function._p_dom_frontiers)
        except AssertionError:
            err_msg = f'Possible noreturn info missing for {self.function}'
            raise NoEntryForDomTreeError(err_msg)

    def _build_dom_sets(self, graph: nx.DiGraph, gname=''):
        v_entry = 'virtual_entry'
        graph.add_edges_from(((v_entry, node) for node, i in tuple(graph.in_degree) if i == 0))
        if v_entry not in graph:
            err_msg = f'Failed to find an entry to build dominance tree for {gname}'
            logging.error(err_msg)
            raise NoEntryForDomTreeError(err_msg)

        dom_tree = nx.DiGraph()
        dom_tree.add_nodes_from(graph.nodes)
        for node, dominator in nx.immediate_dominators(graph, v_entry).items():
            dom_tree.add_edge(dominator, node)
        dom_tree.remove_node(v_entry)

        dominances = {node: set.union(nx.descendants(dom_tree, node), {node, })
                      for node in dom_tree.nodes}

        frontier = nx.dominance_frontiers(graph, v_entry)
        frontier.pop(v_entry)
        graph.remove_node(v_entry)
        return dom_tree, dominances, frontier
