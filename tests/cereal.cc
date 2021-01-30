
#include <cassert>
#include <iostream>
#include <sstream>
#include <xdrpp/printer.h>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include "tests/xdrtest.hh"

#include<boost/type_index.hpp>
using boost::typeindex::type_id_with_cvr;
#define TYPENAME(t) type_id_with_cvr<decltype(t)>().pretty_name()

void
cereal_override(cereal::JSONOutputArchive &ar,
                const testns::elem &e,
                const char* field)
{
    std::cout << "tests/cereal.cc:cereal_override("
              << TYPENAME(ar)
              << ", "
              << TYPENAME(e)
              << ", "
              << (field ? field : "nullptr")
              << ")" << std::endl;
    xdr::archive(ar, std::make_tuple(cereal::make_nvp("valueA", e.a)), field);
//    xdr::archive(
//
//            ar,
//            std::make_tuple(
//                cereal::make_nvp("myValueA", e.a),
//                cereal::make_nvp("myValueB", e.b)),
//            field);
}
void f(cereal::JSONOutputArchive ar)
{
    testns::elem e;
    xdr::xdr_traits<::testns::elem>::save(ar, e);
    testns::arrayInfo a;
    xdr::xdr_traits<::testns::arrayInfo>::save(ar, a);
    xdr::xvector<int, 3> v;
    xdr::xdr_traits<xdr::xvector<int, 3>>::save(ar, v);
    testns::noOverride no;
    xdr::xdr_traits<::testns::noOverride>::save(ar, no);
}


void
cereal_override(cereal::JSONOutputArchive &ar,
                const testns::inner &t,
                const char* field)
{
    xdr::archive(ar, 9999, "bort");
}

#include <xdrpp/cereal.h>

using namespace std;

int
main()
{
//  testns::numerics n1, n2;
//  n1.b = true;
//  n2.b = false;
//  n1.i1 = 0x7eeeeeee;
//  n1.i2 = 0xffffffff;
//  n1.i3 = UINT64_C(0x7ddddddddddddddd);
//  n1.i4 = UINT64_C(0xfccccccccccccccc);
//  n1.f1 = 3.141592654;
//  n1.f2 = 2.71828182846;
//  n1.e1 = testns::REDDER;
//  n2.e1 = testns::REDDEST;
//
//  cout << xdr::xdr_to_string(n1);
//
//  ostringstream obuf;
//  {
//    //cereal::BinaryOutputArchive archive(obuf);
//    cereal::JSONOutputArchive archive(obuf);
//    archive(n1);
//  }
//
//  cout << obuf.str() << endl;
//
//  {
//    istringstream ibuf(obuf.str());
//    //cereal::BinaryInputArchive archive(ibuf);
//    cereal::JSONInputArchive archive(ibuf);
//    archive(n2);
//  }
//
//  cout << xdr::xdr_to_string(n2);
//
//  testns::nested_cereal_adapter_calls nc;
//  nc.strptr.activate() = "hello";
//  nc.strvec.push_back("goodbye");
//  nc.strarr[0] = "friends";
//  {
//    ostringstream obuf2;
//    {
//      cereal::JSONOutputArchive archive(obuf2);
//      archive(nc);
//    }
//    cout << obuf2.str();
//  }
//
//  {
//    testns::outer tu;
//    ostringstream obuf2;
//    {
//      cereal::JSONOutputArchive archive(obuf2);
//      archive(tu);
//    }
//    cout << obuf2.str();
//    assert(obuf2.str().find("\"bort\": 9999") != string::npos);
//  }
  std::cout << std::endl << "================hidenori's output======================" << std::endl;
   {
       testns::elem e;
       e.a = 3;
       e.b = 5;
       ostringstream obuf;
       {
           cereal::JSONOutputArchive ar(obuf);
           xdr::archive(ar, e, "elemInfo"); // works well, cereal_override
 //          ar(e, "elemInfo"); // nonsense
 //          ar(ary.ls.extend_at(0), "elemInfo"); // nonsense
 //          xdr::archive(ar, ary.ls.extend_at(0), "elemInfo"); // works well, cereal_override
       }
       std::cout << obuf.str() << std::endl;
 //      assert(obuf.str() == "{\n    \"valueA\": 3\n}");
   }
  {
      testns::arrayInfo ary;
      ary.id = 123;
      for (int i = 0; i < 5; i++)
      {
          ary.ls.extend_at(i).a = i * i;
          ary.ls.extend_at(i).b = i * i * i;
      }
      ostringstream obuf;
      {
          cereal::JSONOutputArchive ar(obuf);
          xdr::archive(ar, ary, "information");
      }
      std::cout << obuf.str() << std::endl;
      std::cout << "hello world" << std::endl;
  }
// The following gets outputted.
// In other words, the cereal_override for `elem` defined above is ignored.
// {
//     "information": {
//         "id": 123,
//         "ls": [
//             {
//                 "a": 0,
//                 "b": 0
//             },
//             {
//                 "a": 1,
//                 "b": 1
//             },
//             {
//                 "a": 4,
//                 "b": 8
//             },
//             {
//                 "a": 9,
//                 "b": 27
//             },
//             {
//                 "a": 16,
//                 "b": 64
//             }
//         ]
//     }
// }
//
//
// But I want the following:
// {
//     "information": {
//         "id": 123,
//         "ls": [
//             {
//                 "valueA": 0,
//             },
//             {
//                 "valueA": 1,
//             },
//             {
//                 "valueA": 4,
//             },
//             {
//                 "valueA": 9,
//             },
//             {
//                 "valueA": 16,
//             }
//         ]
//     }
// }
//  {
//      testns::noOverride v;
//      v.id = 1;
//      v.e.a = 2;
//      v.e.b = 3;
//      ostringstream obuf;
//      {
//          cereal::JSONOutputArchive ar(obuf);
//          xdr::archive(ar, v, "information");
//          xdr::archive(ar, v, "information");
//      }
//      std::cout << obuf.str() << std::endl;
//  }

  return 0;
}
