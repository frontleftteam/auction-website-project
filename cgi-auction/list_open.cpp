// list_open.cpp (g++ -O2 -std=c++17 list_open.cpp -lsqlite3 -o list_open.cgi)
#include <sqlite3.h>
#include <bits/stdc++.h>
using namespace std;
static const char* DB_PATH="auction.db";
static const char* COOKIE_NAME="AUCTSESS";

string htmlEscape(const string& s){ string o; o.reserve(s.size());
 for(char c:s){ if(c=='&')o+="&amp;"; else if(c=='<')o+="&lt;"; else if(c=='>')o+="&gt;";
 else if(c=='"')o+="&quot;"; else if(c=='\'')o+="&#39;"; else o+=c; } return o; }
string getCookie(const string& name){
    const char* c = getenv("HTTP_COOKIE"); if(!c) return "";
    string cookies(c); size_t p=0;
    while(p<cookies.size()){
        while(p<cookies.size() && cookies[p]==' ') p++;
        size_t eq=cookies.find('=',p); if(eq==string::npos) break;
        string k=cookies.substr(p,eq-p); size_t end=cookies.find(';',eq+1);
        string v=cookies.substr(eq+1,(end==string::npos?cookies.size():end)-(eq+1));
        if(k==name) return v; if(end==string::npos) break; p=end+1;
    } return "";
}
void header(){ cout<<"Content-Type: text/html\r\n\r\n"
"<!doctype html><html><head><meta charset='utf-8'><title>Open Auctions</title>"
"<style>body{font-family:system-ui;margin:2rem} .card{border:1px solid #ddd;border-radius:10px;padding:1rem;margin:.6rem 0}"
".meta{color:#444;font-size:.9rem}</style></head><body>"; }
void footer(){ cout<<"</body></html>"; }

int main(){
    sqlite3* db=nullptr; sqlite3_open(DB_PATH,&db);
    // who am i?
    string sid = getCookie(COOKIE_NAME);
    int uid=-1; string email;
    if(!sid.empty()){
        sqlite3_stmt* st=nullptr;
        sqlite3_prepare_v2(db,"SELECT u.id,u.email FROM sessions s JOIN users u ON u.id=s.user_id WHERE s.id=? AND s.expires_at>datetime('now');",-1,&st,nullptr);
        sqlite3_bind_text(st,1,sid.c_str(),-1,SQLITE_TRANSIENT);
        if(sqlite3_step(st)==SQLITE_ROW){ uid=sqlite3_column_int(st,0); email=(const char*)sqlite3_column_text(st,1); }
        sqlite3_finalize(st);
    }
    header();
    if(uid>0) cout<<"<p>Signed in as <b>"<<htmlEscape(email)<<"</b> • <a href='/cgi-bin/transactions.cgi'>Your transactions</a> • <a href='/cgi-bin/bid_sell.cgi'>Bid/Sell</a></p>";
    else cout<<"<p><a href='/cgi-bin/auth.cgi'>Log in</a> to bid.</p>";

    // list auctions
    sqlite3_stmt* st=nullptr;
    sqlite3_prepare_v2(db,
       "SELECT a.id,a.title,a.description,a.start_price,a.start_time,a.end_time,a.seller_id,"
       " COALESCE((SELECT MAX(b.max_bid) FROM bids b WHERE b.auction_id=a.id), a.start_price) AS current_price "
       "FROM auctions a WHERE datetime('now') BETWEEN a.start_time AND a.end_time "
       "ORDER BY a.end_time ASC;",-1,&st,nullptr);

    cout<<"<h1>Open Auctions (ending soonest)</h1>";
    while(sqlite3_step(st)==SQLITE_ROW){
        int id=sqlite3_column_int(st,0);
        const char* title=(const char*)sqlite3_column_text(st,1);
        const char* desc=(const char*)sqlite3_column_text(st,2);
        double start=sqlite3_column_double(st,3);
        const char* stime=(const char*)sqlite3_column_text(st,4);
        const char* etime=(const char*)sqlite3_column_text(st,5);
        int seller=sqlite3_column_int(st,6);
        double cur=sqlite3_column_double(st,7);
        cout<<"<div class='card'><h3>"<<htmlEscape(title?title:"(untitled)")<<"</h3>";
        cout<<"<div class='meta'>ID "<<id<<" • Ends: "<<(etime?etime:"")<<" • Started: "<<(stime?stime:"")<<"</div>";
        cout<<"<p>"<<htmlEscape(desc?desc:"")<<"</p>";
        cout<<"<p><b>Current price:</b> "<<fixed<<setprecision(2)<<cur<<"</p>";
        if(uid>0 && uid!=seller){
            cout<<"<form method='GET' action='/cgi-bin/bid_sell.cgi'>"
                  "<input type='hidden' name='prefill_auction_id' value='"<<id<<"'>"
                  "<button type='submit'>Bid</button></form>";
        } else if(uid>0 && uid==seller){
            cout<<"<p><i>Your item (you cannot bid).</i></p>";
        }
        cout<<"</div>";
    }
    sqlite3_finalize(st);
    footer();
    sqlite3_close(db);
    return 0;
}
