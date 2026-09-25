#!/usr/bin/env python3
"""生成发音评分适配器测试用的假 CTC ONNX 模型与配套词表。

为什么手写 protobuf：CI 纪律（docs/pronunciation-plan.md）是"适配器层
做编译验证不跑模型"，但一个 1.5KB 的 Constant 模型能让会话加载/推理/
张量解析/后处理全链路进测试，且不需要 635MB 真模型，也不需要 python
onnx 包（py3.14 源码编译半小时起）。ONNX 就是 protobuf，图只有
"Constant → logits"一个节点，手工编码几行搞定，确定性还更好。

输出（--out 目录，默认 tests/fixtures）：
  fake_ctc.onnx   Constant 图，输出 (1,16,8) fp32 logits：
                  帧 0-1 偏向 blank(<pad>)，2-6 偏向 k，7-11 偏向 æ，
                  12-15 偏向 t（ favored logit 12，其余 0）
  fake_vocab.json 8 类词表：{"<pad>":0,"k":1,"æ":2,"t":3,"s":4,
                  "ə":5,"ɪ":6,"u":7}
"""

import argparse
import json
import struct

# ---------------- protobuf 编码原语 ----------------


def varint(n: int) -> bytes:
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def tag(field: int, wire: int) -> bytes:
    return varint((field << 3) | wire)


def v(field: int, value: int) -> bytes:
    """varint 字段"""
    return tag(field, 0) + varint(value)


def ld(field: int, payload: bytes) -> bytes:
    """length-delimited 字段（string/bytes/嵌套消息）"""
    return tag(field, 2) + varint(len(payload)) + payload


# ---------------- ONNX 消息拼装 ----------------


def tensor_proto(dims, raw_data: bytes, data_type: int = 1) -> bytes:
    out = b""
    for d in dims:
        out += v(1, d)  # dims（repeated int64，非 packed——老编码，兼容面最大）
    out += v(2, data_type)  # 1 = FLOAT
    out += ld(9, raw_data)  # raw_data
    return out


def attribute_value_tensor(t: bytes) -> bytes:
    # name="value", t=TensorProto, type=4(TENSOR)——ORT 新版要求显式
    # attribute type，缺省(0/UNDEFINED)会拒收
    return ld(1, b"value") + ld(5, t) + v(20, 4)


def node_constant(output: str, tensor: bytes) -> bytes:
    return (
        ld(2, output.encode())          # output
        + ld(3, b"const_logits")        # name
        + ld(4, b"Constant")            # op_type
        + ld(5, attribute_value_tensor(tensor))  # attribute
    )


def value_info_unknown_rank3(name: str) -> bytes:
    # ValueInfoProto{name, TypeProto{tensor_type: Tensor{FLOAT, shape(3 个未知维)}}}
    shape = ld(1, b"") * 3  # 3 个 dim=[]（未知维）
    tensor_msg = v(1, 1) + ld(2, shape)  # Tensor.elem_type=FLOAT, Tensor.shape
    type_msg = ld(1, tensor_msg)  # TypeProto.tensor_type
    return ld(1, name.encode()) + ld(2, type_msg)  # ValueInfoProto.name/.type


def graph(node: bytes, name: str, outputs) -> bytes:
    out = ld(1, node) + ld(2, name.encode())
    for o in outputs:
        out += ld(12, o)  # output（ValueInfoProto）
    return out


def model_proto(g: bytes, opset: int = 17, ir_version: int = 8) -> bytes:
    opset_import = v(2, opset)  # domain 省略 = 默认 ONNX 域
    return v(1, ir_version) + ld(7, g) + ld(8, opset_import)


# ---------------- fixture 语义 ----------------

NUM_FRAMES = 16
NUM_CLASSES = 8
# 类布局与词表一致；blank = <pad> = 0（HF wav2vec2 惯例，真模型相同）
VOCAB = {"<pad>": 0, "k": 1, "æ": 2, "t": 3, "s": 4, "ə": 5, "ɪ": 6, "u": 7}
# 每帧 favored 类：0-1 blank，2-6 k，7-11 æ，12-15 t
FRAME_PHONE = (
    [0, 0] + [1] * 5 + [2] * 5 + [3] * 4
)
assert len(FRAME_PHONE) == NUM_FRAMES


def logits_raw() -> bytes:
    rows = []
    for favored in FRAME_PHONE:
        row = [12.0 if c == favored else 0.0 for c in range(NUM_CLASSES)]
        rows.append(struct.pack(f"<{NUM_CLASSES}f", *row))
    return b"".join(rows)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="tests/fixtures")
    args = ap.parse_args()

    tensor = tensor_proto([1, NUM_FRAMES, NUM_CLASSES], logits_raw())
    node = node_constant("logits", tensor)
    g = graph(node, "fake_ctc", [value_info_unknown_rank3("logits")])
    blob = model_proto(g)

    import os
    os.makedirs(args.out, exist_ok=True)
    onnx_path = os.path.join(args.out, "fake_ctc.onnx")
    vocab_path = os.path.join(args.out, "fake_vocab.json")
    with open(onnx_path, "wb") as f:
        f.write(blob)
    with open(vocab_path, "w", encoding="utf-8") as f:
        json.dump(VOCAB, f, ensure_ascii=False)
    print(f"wrote {onnx_path} ({len(blob)} bytes), {vocab_path}")


if __name__ == "__main__":
    main()
