// Copyright 2025 Oleg Maximenko
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include "test_helpers.h"

using gql::test::Field;

TEST(SyntaxAnalyzer, GraphPattern) {
  GQL_TEST_PARSE("MATCH (a)-(b)").ExpectOmittedResult();
  GQL_TEST_PARSE("MATCH (a)-(b) RETURN *")
      .ExpectTableResult()
      .WithFields({Field("a").NodeReference().Unconditional(),
                   Field("b").NodeReference().Unconditional()});
  GQL_TEST_PARSE("MATCH (a)-(b)-(c) YIELD a, b RETURN *")
      .ExpectTableResult()
      .WithFields({Field("a").NodeReference().Unconditional(),
                   Field("b").NodeReference().Unconditional()});
  GQL_TEST_PARSE("MATCH (a)-(b) | (a)-()-[e:LABEL1]-(c) RETURN *")
      .ExpectTableResult()
      .WithFields({Field("a").NodeReference().Unconditional(),
                   Field("b").NodeReference().Conditional(),
                   Field("c").NodeReference().Conditional(),
                   Field("e").EdgeReference().Conditional()});
  GQL_TEST_PARSE("MATCH (a)-b)").ExpectError();
  GQL_TEST_PARSE("MATCH (a)-(b)*").ExpectError().Contains("bound");
  GQL_TEST_PARSE("MATCH path1 = (a)-(b), path1 = (c)")
      .ExpectErrorContaining("path variable");
  GQL_TEST_PARSE("MATCH (a WHERE a.prop=5)-((b)-(c) WHERE b.prop1=c.prop2)");
  GQL_TEST_PARSE("MATCH (a), (a)");
  GQL_TEST_PARSE("MATCH (a), (b WHERE b.prop=a.prop)");
  GQL_TEST_PARSE("MATCH (a), ANY SHORTEST (b WHERE b.prop=a.prop)")
      .ExpectErrorContaining("selective");
  GQL_TEST_PARSE("MATCH (a)-(b), ANY SHORTEST (a)-(e)-(b)");
  GQL_TEST_PARSE("MATCH (a)-(b), ANY SHORTEST (a)-(b)-(f)")
      .ExpectErrorContaining("strict interior");
  GQL_TEST_PARSE("MATCH (a)?-(b)?").ExpectErrorContaining("length");
  GQL_TEST_PARSE("MATCH (a)-(b), (a)(-(b))?").ExpectErrorContaining("degree");
  GQL_TEST_PARSE("MATCH ((a)-())?-(b), ((a)-())?-(b)")
      .ExpectErrorContaining("degree");
  GQL_TEST_PARSE("MATCH (-[a]-)?(b), (-[a]-)?(b)")
      .ExpectErrorContaining("degree");
  GQL_TEST_PARSE("MATCH (a)+, (a)+").ExpectErrorContaining("restrictive");
  GQL_TEST_PARSE("MATCH ((a)-){1,4}, ((a)-){1,4}")
      .ExpectErrorContaining("degree");
  GQL_TEST_PARSE("MATCH (a)-(b) | (c WHERE c.prop=d.prop)")
      .ExpectErrorContaining("unknown");
  GQL_TEST_PARSE("MATCH (a)-(b) | (c WHERE c.prop=a.prop)")
      .ExpectErrorContaining("union");
  GQL_TEST_PARSE("MATCH (a)-(b) ((e) | (c WHERE c.prop=a.prop))");
  GQL_TEST_PARSE("MATCH (a)-(((b)-){10})-(((c)-){11} WHERE COUNT(c) < 7)");
  GQL_TEST_PARSE("MATCH (a)-(((b)-){10})-(((c)-){11} WHERE COUNT(b) < 7)")
      .ExpectErrorContaining("group degree");
  GQL_TEST_PARSE("MATCH (a)-((b)-){10} WHERE (COUNT(b) < 7)");
  GQL_TEST_PARSE("MATCH (a)-((b)-){10} WHERE (COUNT(a) < 7)")
      .ExpectErrorContaining("aggregate function");
  GQL_TEST_PARSE("MATCH (a)-(subpath1 = ((b)-){0,3})")
      .ExpectErrorContaining("minimum node count");
  GQL_TEST_PARSE("MATCH ((a)-){0,3} RETURN NO BINDINGS")
      .ExpectErrorContaining("minimum node count");
  GQL_TEST_PARSE(
      "MATCH ((A)->((B)->(C) WHERE A.X = B.X + C.X)->(D)){2} RETURN *")
      .ExpectTableResult()
      .WithFields(
          {Field("A").NodeReferenceGroup(), Field("B").NodeReferenceGroup(),
           Field("C").NodeReferenceGroup(), Field("D").NodeReferenceGroup()});
}

TEST(SyntaxAnalyzer, GraphPatternGroupBy) {
  GQL_TEST_PARSE(
      "MATCH (a)-(b) RETURN a.id AS a_id, b.prop AS b_prop GROUP BY a_id")
      .ExpectErrorContaining("aggregating expression");
  GQL_TEST_PARSE(
      "MATCH (a)-(b)-(c) RETURN a.id AS a_id, SUM(b.prop1) AS b_prop, "
      "SUM(c.prop2) AS c_prop GROUP BY a_id")
      .ExpectTableResult()
      .WithFields({Field("a_id"), Field("b_prop"), Field("c_prop")});
}