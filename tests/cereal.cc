
#include <cassert>
#include <iostream>
#include <sstream>
#include <xdrpp/printer.h>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include "tests/xdrtest.hh"

void
cereal_override(cereal::JSONOutputArchive &ar,
                const testns::inner &t,
                const char* field)
{
    xdr::archive(ar, 9999, "bort");
}

void
cereal_override(cereal::JSONOutputArchive &ar,
                const testns::elem &t,
                const char* field)
{
    xdr::archive(ar,
                 std::make_tuple(cereal::make_nvp(std::string("Custom Member ID"), t.memberId),
                                 cereal::make_nvp(std::string("Custom Member name"), t.memberName)),
                 field);
}

template <typename T>
std::enable_if_t<xdr::xdr_traits<T>::is_container>
cereal_override(cereal::JSONOutputArchive& ar, T const& t,
                const char* field)
{
    ar.setNextName(field);
    ar.startNode();
    ar(cereal::make_size_tag(0));
    for (auto const& element : t)
    {
        xdr::archive(ar, element);
    }
    ar.finishNode();
}


#include <xdrpp/cereal.h>

using namespace std;

int
main()
{
  testns::numerics n1, n2;
  n1.b = true;
  n2.b = false;
  n1.i1 = 0x7eeeeeee;
  n1.i2 = 0xffffffff;
  n1.i3 = UINT64_C(0x7ddddddddddddddd);
  n1.i4 = UINT64_C(0xfccccccccccccccc);
  n1.f1 = 3.141592654;
  n1.f2 = 2.71828182846;
  n1.e1 = testns::REDDER;
  n2.e1 = testns::REDDEST;

  cout << xdr::xdr_to_string(n1);

  ostringstream obuf;
  {
    //cereal::BinaryOutputArchive archive(obuf);
    cereal::JSONOutputArchive archive(obuf);
    archive(n1);
  }

  cout << obuf.str() << endl;

  {
    istringstream ibuf(obuf.str());
    //cereal::BinaryInputArchive archive(ibuf);
    cereal::JSONInputArchive archive(ibuf);
    archive(n2);
  }

  cout << xdr::xdr_to_string(n2);

  testns::nested_cereal_adapter_calls nc;
  nc.strptr.activate() = "hello";
  nc.strvec.push_back("goodbye");
  nc.strarr[0] = "friends";
  {
    ostringstream obuf2;
    {
      cereal::JSONOutputArchive archive(obuf2);
      archive(nc);
    }
    cout << obuf2.str();
  }

  {
    testns::outer tu;
    ostringstream obuf2;
    {
      cereal::JSONOutputArchive archive(obuf2);
      archive(tu);
    }
    cout << obuf2.str();
    assert(obuf2.str().find("\"bort\": 9999") != string::npos);
  }

  {
      testns::elem e;
      e.memberId = 3;
      e.memberName = 500;
      ostringstream obuf;
      {
          cereal::JSONOutputArchive ar(obuf);
          xdr::archive(ar, e, "SingleElement");
      }
      std::cout << obuf.str() << std::endl;
  }

  {
      testns::array1 ary1;
      ary1.id = 123;
      for (int j = 0; j < 3; j++)
      {
          ary1.ary1.extend_at(j).memberId = j;
          ary1.ary1.extend_at(j).memberName = 1000 + j;
      }
      ostringstream obuf;
      {
          cereal::JSONOutputArchive ar(obuf);
          xdr::archive(ar, ary1, "Flat Array");
      }
      std::cout << obuf.str() << std::endl;
  }
  {
      testns::array2 a;
      a.id = 123;
      for (int i = 0; i < 3; i++)
      {
          a.ary2.extend_at(i).id = 100 * i;
          for (int j = 0; j < 3; j++)
          {
              a.ary2.extend_at(i).ary1.extend_at(j).memberId = 10 * i + j;
              a.ary2.extend_at(i).ary1.extend_at(j).memberName = 1000 * i + j;
          }
      }
      ostringstream obuf;
      {
          cereal::JSONOutputArchive ar(obuf);
          xdr::archive(ar, a, "nestedArray");
      }
      std::cout << obuf.str() << std::endl;
  }

  return 0;
}
