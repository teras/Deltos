#!/usr/bin/env python3
"""Rewrite the DocAligner model so that OpenCV 4 can read it.

The model as published by DocsaidLab uses two constructs the DNN importer of
OpenCV 4 rejects, both of them in the BiFPN neck's weighted sums:

  * a Relu over a learnt parameter, which the importer cannot give a shape to
    ("getMemoryShapes: inputs.size()"), and
  * Einsum with the equation "i,i...->...", which it does not implement at all.

Both belong to the same expression: the neck scales each of its inputs by a
learnt weight, normalised across the inputs. Those weights are parameters, so
at inference the whole branch is constant. This script evaluates it once with
onnxruntime, bakes the numbers in, and writes the weighted sum out as plain
multiplications and additions, which every OpenCV understands. The graph is
then pruned of what nothing reaches any more.

The result is arithmetically identical -- verified against the original with
onnxruntime, to the last bit -- and OpenCV 5 produces exactly the same
detection from it as before. It is only the way the same computation is
spelled out that changes.

    pip install onnx onnxruntime
    python3 rewrite-for-opencv4.py original.onnx rewritten.onnx
"""
import sys
import numpy as np
import onnx
import onnxruntime as ort
from onnx import helper, numpy_helper

src, dst = sys.argv[1], sys.argv[2]
model = onnx.load(src)
graph = model.graph
producer = {o: n for n in graph.node for o in n.output}

einsums = [n for n in graph.node if n.op_type == "Einsum"]
if not einsums:
    sys.exit("no Einsum nodes: nothing to rewrite")

# Evaluate the weight branch once. Any input will do, since it does not depend
# on one; the shape has to be the model's own.
probe = onnx.load(src)
del probe.graph.output[:]
for name in dict.fromkeys(n.input[0] for n in einsums):
    probe.graph.output.append(helper.make_empty_tensor_value_info(name))
session = ort.InferenceSession(probe.SerializeToString(), providers=["CPUExecutionProvider"])
weights = dict(zip([o.name for o in session.get_outputs()],
                   session.run(None, {session.get_inputs()[0].name:
                                      np.zeros((1, 3, 256, 256), np.float32)})))

initializers, replacement, replaced = [], {}, set()
for node in einsums:
    equation = next(a.s.decode() for a in node.attribute if a.name == "equation")
    if equation != "i,i...->...":
        sys.exit(f"unexpected einsum equation {equation!r}")
    w = weights[node.input[0]]
    concat = producer[node.input[1]]
    parts = [producer[i].input[0] for i in concat.input]   # the tensors before they were stacked
    if len(parts) != len(w):
        sys.exit(f"{node.name}: {len(parts)} inputs but {len(w)} weights")

    base = node.name.replace("/", "_").lstrip("_")
    nodes, terms = [], []
    for i, source in enumerate(parts):
        name = f"{base}_w{i}"
        initializers.append(numpy_helper.from_array(np.array([w[i]], np.float32), name))
        nodes.append(helper.make_node("Mul", [source, name], [f"{base}_m{i}"], name=f"{base}_mul{i}"))
        terms.append(f"{base}_m{i}")
    total = terms[0]
    for k in range(1, len(terms)):
        out = node.output[0] if k == len(terms) - 1 else f"{base}_acc{k}"
        nodes.append(helper.make_node("Add", [total, terms[k]], [out], name=f"{base}_add{k}"))
        total = out
    replacement[node.name] = nodes
    replaced.add(node.name)

rewritten = []
for node in graph.node:
    if node.name in replacement:
        rewritten.extend(replacement[node.name])
    elif node.name not in replaced:
        rewritten.append(node)

# Keep only what the outputs still reach: this is what drops the Relu.
produced = {o: n for n in rewritten for o in n.output}
needed, pending = set(), [o.name for o in graph.output]
while pending:
    node = produced.get(pending.pop())
    if node is None or node.name in needed:
        continue
    needed.add(node.name)
    pending.extend(node.input)
kept = [n for n in rewritten if n.name in needed]

del graph.node[:]
graph.node.extend(kept)
graph.initializer.extend(initializers)
used = {i for n in kept for i in n.input}
still = [t for t in graph.initializer if t.name in used]
del graph.initializer[:]
graph.initializer.extend(still)

onnx.checker.check_model(model, full_check=False)
onnx.save(model, dst)
print(f"{len(graph.node)} nodes written to {dst}")
