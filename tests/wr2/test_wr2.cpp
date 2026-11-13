#include "../../modules/wr2/WeightedRandom.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <vector>
using namespace wr2;
static unsigned long long checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
static bool same(const InstrumentState& a,const InstrumentState& b) {
    return std::memcmp(&a,&b,sizeof(a)) == 0;
}
static InstrumentState group(std::initializer_list<unsigned> weights) {
    InstrumentState s; s.reset(static_cast<unsigned>(weights.size()));
    unsigned i=0; for (auto w:weights) { s.owner[i]=0; s.weight[i++]=static_cast<std::uint8_t>(w); }
    CHECK(valid(s)); return s;
}
// Independent oracle: sort all rational remainders rather than scanning/awarding.
static std::vector<unsigned> oracle(const std::vector<unsigned>& weights,unsigned target) {
    unsigned sum=0; for(auto w:weights)sum+=w;
    std::vector<unsigned> result(weights.size());
    std::vector<std::pair<unsigned,unsigned>> order;
    unsigned used=0;
    for(unsigned i=0;i<weights.size();++i) {
        const unsigned numerator=sum?weights[i]*target:target;
        const unsigned denominator=sum?sum:static_cast<unsigned>(weights.size());
        result[i]=numerator/denominator; used+=result[i];
        order.emplace_back(numerator%denominator,i);
    }
    std::sort(order.begin(),order.end(),[](auto a,auto b){return a.first!=b.first?a.first>b.first:a.second<b.second;});
    for(unsigned i=0;i<target-used;++i)++result[order[i].second];
    return result;
}
static void actions() {
    InstrumentState s; CHECK(valid(s)); CHECK(s.sliceCount==0);
    CHECK(!s.reset(49)); CHECK(valid(s)); CHECK(s.reset(48));
    CHECK(create(s,0)==Result::Applied); CHECK(s.weight[0]==100);
    CHECK(setWeight(s,0,0,0)==Result::Unchanged); CHECK(s.weight[0]==100);
    CHECK(add(s,0,1)==Result::Applied); CHECK(s.weight[0]==90 && s.weight[1]==10);
    CHECK(add(s,0,2)==Result::Applied); CHECK(s.weight[0]==81 && s.weight[1]==9 && s.weight[2]==10);
    CHECK(add(s,0,2)==Result::Unchanged);
    CHECK(create(s,5)==Result::Applied); auto before=s;
    CHECK(add(s,0,5)==Result::ForeignGroup); CHECK(same(s,before));
    CHECK(setWeight(s,0,1,100)==Result::Applied);
    CHECK(s.weight[0]==0 && s.weight[1]==100 && s.weight[2]==0);
    CHECK(setWeight(s,0,1,1)==Result::Applied);
    CHECK(s.weight[0]==50 && s.weight[1]==1 && s.weight[2]==49);
    before=s; CHECK(add(s,0,3)==Result::WouldMute); CHECK(same(s,before));
    CHECK(setWeight(s,0,0,101)==Result::Applied); CHECK(s.weight[0]==100);
    CHECK(setWeight(s,0,0,-1)==Result::Applied); CHECK(s.weight[1]==50 && s.weight[2]==50);
    CHECK(remove(s,0,5)==Result::Unchanged); CHECK(s.weight[5]==100);
    CHECK(remove(s,0,0)==Result::Applied);
    CHECK(s.owner[0]==kFree && s.owner[1]==kFree && s.owner[2]==kFree);
    CHECK(s.owner[5]==5 && valid(s));
    s=group({0,100,0}); CHECK(remove(s,0,1)==Result::Applied);
    CHECK(s.weight[0]==50 && s.weight[2]==50 && valid(s));
    s=group({100,0}); s.reset(3); create(s,0); add(s,0,1); setWeight(s,0,0,100);
    CHECK(add(s,0,2)==Result::Applied); CHECK(s.weight[1]==0 && s.weight[0]==90 && s.weight[2]==10);
    ProjectState bank; bank.instrument[0].reset(48); bank.instrument[47].reset(48);
    create(bank.instrument[0],47); bank.instrument[47]=bank.instrument[0];
    remove(bank.instrument[47],47,47); CHECK(bank.instrument[0].owner[47]==47);
    CHECK(bank.instrument[1].sliceCount==0);
    CHECK(sizeof(bank)==4656);
    // Maximum 48-member group is representable independently of Add refusal.
    s.reset(48); for(unsigned i=0;i<48;++i){s.owner[i]=47;s.weight[i]=2;}
    s.weight[0]=6; CHECK(valid(s));
    CHECK(setWeight(s,47,47,100)==Result::Applied); CHECK(valid(s));
    CHECK(remove(s,47,47)==Result::Applied); CHECK(valid(s));
}
static void exhaustiveHamilton() {
    for(unsigned a=0;a<=100;++a)for(unsigned b=0;b<=100-a;++b) {
        const unsigned c=100-a-b;
        auto initial=group({a,b,c});
        for(unsigned selected=0;selected<3;++selected)for(unsigned w=0;w<=100;++w) {
            auto s=initial; setWeight(s,0,selected,static_cast<int>(w)); CHECK(valid(s));
            std::vector<unsigned> peers; for(unsigned i=0;i<3;++i)if(i!=selected)peers.push_back(initial.weight[i]);
            auto expected=oracle(peers,100-w); unsigned j=0;
            CHECK(s.weight[selected]==w);
            for(unsigned i=0;i<3;++i)if(i!=selected)CHECK(s.weight[i]==expected[j++]);
        }
        for(unsigned selected=1;selected<3;++selected) {
            auto s=initial; CHECK(remove(s,0,selected)==Result::Applied);
            std::vector<unsigned> peers; for(unsigned i=0;i<3;++i)if(i!=selected)peers.push_back(initial.weight[i]);
            auto expected=oracle(peers,100); unsigned j=0;
            for(unsigned i=0;i<3;++i)if(i!=selected)CHECK(s.weight[i]==expected[j++]);
            CHECK(valid(s));
        }
        auto s=initial; s.sliceCount=4;
        bool refuse=(a && a*90/100==0)||(b && b*90/100==0)||(c && c*90/100==0);
        auto before=s; auto result=add(s,0,3);
        CHECK(result==(refuse?Result::WouldMute:Result::Applied));
        if(refuse)CHECK(same(s,before));
        else {auto expected=oracle({a,b,c},90);for(unsigned i=0;i<3;++i)CHECK(s.weight[i]==expected[i]);CHECK(s.weight[3]==10);CHECK(valid(s));}
    }
}
static void selection() {
    auto s=group({60,30,10}); std::array<unsigned,3> hits{};
    for(unsigned ticket=0;ticket<100;++ticket)++hits[selectTicket(s,0,ticket)];
    CHECK(hits[0]==60 && hits[1]==30 && hits[2]==10);
    s=group({0,100,0}); for(unsigned t=0;t<100;++t)CHECK(selectTicket(s,0,t)==1);
    for(unsigned t=0;t<100;++t)CHECK(selectTicket(s,2,t)==2);
    Random r(1); auto state=r.state();
    CHECK(select(s,0,3,true,true,r)==0); CHECK(r.state()==state);
    CHECK(select(s,0,3,false,false,r)==0); CHECK(r.state()==state);
    CHECK(select(s,0,2,true,false,r)==0); CHECK(r.state()==state);
    CHECK(select(s,1,3,true,false,r)==1); CHECK(r.state()==state);
    CHECK(select(s,0,3,true,false,r)==1); CHECK(r.state()==270369u);
    CHECK(selectTicket(s,0,100)==0); CHECK(selectTicket(s,999,0)==999);
    s.reset(3); state=r.state(); CHECK(select(s,0,3,true,false,r)==0); CHECK(r.state()==state);
    Random zero(0); CHECK(zero.state()!=0);
    // Fixed seeded statistical test: separate from the CE Chance RNG.
    s=group({60,30,10}); Random rng(0x12345678), duplicate(0x12345678);
    std::array<unsigned,3> counts{}; unsigned last=99,repeats=0;
    constexpr unsigned n=1000000;
    for(unsigned i=0;i<n;++i){auto x=select(s,0,3,true,false,rng);CHECK(x<3);CHECK(x==select(s,0,3,true,false,duplicate));++counts[x];if(x==last)++repeats;last=x;}
    double chi=0;const double p[]={.6,.3,.1};for(unsigned i=0;i<3;++i){double d=counts[i]-n*p[i];chi+=d*d/(n*p[i]);}
    CHECK(chi<25.0);CHECK(repeats>0);
    std::printf("distribution n=%u: %u/%u/%u; chi2=%.6f; consecutive repeats=%u\n",n,counts[0],counts[1],counts[2],chi,repeats);
}
static void invalidInputs() {
    const auto good=group({60,30,10});
    for(unsigned pos=0;pos<48;++pos)for(unsigned byte=0;byte<256;++byte)for(unsigned field=0;field<2;++field) {
        auto s=good; if(field==0)s.owner[pos]=static_cast<std::uint8_t>(byte);else s.weight[pos]=static_cast<std::uint8_t>(byte);
        if(valid(s))continue;
        Random r(7); auto before=s;
        CHECK(select(s,0,3,true,false,r)==0);CHECK(r.state()==7);
        CHECK(create(s,0)==Result::Invalid);CHECK(add(s,0,1)==Result::Invalid);
        CHECK(setWeight(s,0,1,50)==Result::Invalid);CHECK(remove(s,0,1)==Result::Invalid);CHECK(same(s,before));
    }
    for(unsigned count=49;count<256;++count){auto s=good;s.sliceCount=static_cast<std::uint8_t>(count);CHECK(!valid(s));CHECK(selectTicket(s,0,0)==0);}
    for(auto index:{48u,255u,65535u,std::numeric_limits<unsigned>::max()}){
        auto s=good;CHECK(create(s,index)==Result::Invalid);CHECK(add(s,index,0)==Result::Invalid);CHECK(add(s,0,index)==Result::Invalid);
        CHECK(setWeight(s,index,0,1)==Result::Invalid);CHECK(setWeight(s,0,index,1)==Result::Invalid);
        CHECK(remove(s,index,0)==Result::Invalid);CHECK(remove(s,0,index)==Result::Invalid);CHECK(same(s,good));
    }
    auto s=good;s.owner[0]=1;s.owner[1]=0;CHECK(!valid(s));
}
static void randomEdits() {
    std::mt19937 rng(0x574232u);ProjectState bank;
    for(auto& s:bank.instrument)s.reset(48);
    for(unsigned iteration=0;iteration<100000;++iteration) {
        unsigned slot=rng()%48,master=rng()%50,slice=rng()%50,op=rng()%4;
        auto& s=bank.instrument[slot];auto before=s;Result result;
        switch(op){case 0:result=create(s,slice);break;case 1:result=add(s,master,slice);break;
            case 2:result=setWeight(s,master,slice,static_cast<int>(rng()%141)-20);break;
            default:result=remove(s,master,slice);break;}
        CHECK(valid(s));if(result!=Result::Applied)CHECK(same(s,before));
    }
}
int main(){actions();exhaustiveHamilton();selection();invalidInputs();randomEdits();
    std::printf("PASS: %llu checks; state=%zu bytes; project=%zu bytes\n",checks,sizeof(InstrumentState),sizeof(ProjectState));}
