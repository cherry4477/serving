/* Copyright 2016 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_serving/session_bundle/session_bundle.h"

#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include <gtest/gtest.h>
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/lib/io/path.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/core/public/session_options.h"
#include "tensorflow_serving/session_bundle/signature.h"
#include "tensorflow_serving/test_util/test_util.h"

namespace tensorflow {
namespace serving {
namespace {

void BasicTest(const string& export_path) {
  tensorflow::SessionOptions options;
  SessionBundle bundle;
  TF_ASSERT_OK(LoadSessionBundleFromPath(options, export_path, &bundle));

  const string asset_path =
      tensorflow::io::JoinPath(export_path, kAssetsDirectory);
  // Validate the assets behavior.
  std::vector<Tensor> path_outputs;
  TF_ASSERT_OK(bundle.session->Run({}, {"filename1:0", "filename2:0"}, {},
                                   &path_outputs));
  ASSERT_EQ(2, path_outputs.size());
  // Validate the two asset file tensors are set by the init_op and include the
  // base_path and asset directory.
  test::ExpectTensorEqual<string>(
      test::AsTensor<string>(
          {tensorflow::io::JoinPath(asset_path, "hello1.txt")},
          TensorShape({})),
      path_outputs[0]);
  test::ExpectTensorEqual<string>(
      test::AsTensor<string>(
          {tensorflow::io::JoinPath(asset_path, "hello2.txt")},
          TensorShape({})),
      path_outputs[1]);

  // Validate the half plus two behavior.
  Tensor input = test::AsTensor<float>({0, 1, 2, 3}, TensorShape({4, 1}));

  // Recover the Tensor names of our inputs and outputs.
  Signatures signatures;
  TF_ASSERT_OK(GetSignatures(bundle.meta_graph_def, &signatures));
  ASSERT_TRUE(signatures.default_signature().has_regression_signature());
  const tensorflow::serving::RegressionSignature regression_signature =
      signatures.default_signature().regression_signature();

  const string input_name = regression_signature.input().tensor_name();
  const string output_name = regression_signature.output().tensor_name();

  std::vector<Tensor> outputs;
  TF_ASSERT_OK(
      bundle.session->Run({{input_name, input}}, {output_name}, {}, &outputs));
  ASSERT_EQ(outputs.size(), 1);
  test::ExpectTensorEqual<float>(
      outputs[0], test::AsTensor<float>({2, 2.5, 3, 3.5}, TensorShape({4, 1})));
}

// Test using an exported model from tensorflow/contrib.
TEST(LoadSessionBundleFromPath, BasicTensorflowContrib) {
  const string export_path = tensorflow::io::JoinPath(
      getenv("TEST_SRCDIR"),
      "tf_serving/external/org_tensorflow/tensorflow/"
      "contrib/session_bundle/example/half_plus_two/00000123");
  BasicTest(export_path);
}

// Test using an exported model from tensorflow_serving.
TEST(LoadSessionBundleFromPath, BasicTensorflowServing) {
  const string export_path = test_util::TestSrcDirPath(
      "session_bundle/example/half_plus_two/00000123");
  BasicTest(export_path);
}

TEST(LoadSessionBundleFromPath, BadExportPath) {
  const string export_path = test_util::TestSrcDirPath("/tmp/bigfoot");
  tensorflow::SessionOptions options;
  options.target = "local";
  SessionBundle bundle;
  const auto status = LoadSessionBundleFromPath(options, export_path, &bundle);
  ASSERT_FALSE(status.ok());
  const string msg = status.ToString();
  EXPECT_TRUE(msg.find("Not found") != std::string::npos) << msg;
}

}  // namespace
}  // namespace serving
}  // namespace tensorflow
