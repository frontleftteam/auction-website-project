// bid_sell.cpp  (g++ -O2 -std=c++17 bid_sell.cpp -lsqlite3 -o bid_sell.cgi)
#include <sqlite3.h>
#include <bits/stdc++.h>
using namespace std;
static const char* DB_PATH="auction.db";
static const char* COOKIE_NAME="AUCTSESS";

string htmlEscape(const string& s){ string o; o.reserve(s.size());
 for(char c:s){ if(c=='&')o+="&amp;"; else if(c=='<')o+="&lt;"; else if(c=='>')o+="&gt;";
 else if(c=='"')o+="&quot;"; else if(c=='\'')o+="&#39;"; else o+=c; } return o; }
string urlDecode(const string &in){ string out; out.reserve(in.size());
 for (size_t i=0;i<in.size();++i){ if(in[i]=='+') out.push_back(' ');
 else if(in[i]=='%'&&i+2<in.size()){int v=0; sscanf(in.substr(i+1,2).c_str(),"%x",&v); out.push_back((char)v); i+=2;}
 else out.push_back(in[i]); } return out; }
map<string,string> parseURLEncoded(const string& body){ map<string,string> m; size_t s=0;
 while(s<body.size()){ size_t eq=body.find('=',s); size_t amp=body.find('&',s); if(eq==string::npos) break;
 string k=urlDecode(body.substr(s,eq-s)); string v=urlDecode(body.substr(eq+1,(amp==string::npos?body.size():amp)-eq-1)); m[k]=v;
 if(amp==string::npos) break; s=amp+1;} return m; }
string readStdin(size_t n){ string s; s.resize(n); if(n>0) fread(&s[0],1,n,stdin); return s; }

string getCookie(const string& name){
    const char* c = getenv("HTTP_COOKIE"); if(!c) return "";
    string cookies(c);
    size_t p=0;
    while(p<cookies.size()){
        while(p<cookies.size() && cookies[p]==' ') p++;
        size_t eq = cookies.find('=',p);
        if(eq==string::npos) break;
        string k = cookies.substr(p,eq-p);
        size_t end = cookies.find(';',eq+1);
        string v = cookies.substr(eq+1, (end==string::npos? cookies.size():end)-(eq+1));
        if(k==name) return v;
        if(end==string::npos) break;
        p = end+1;
    }
    return "";
}
void header(const string& extra=""){ cout<<"Content-Type: text/html\r\n"<<extra<<"\r\n"; 
 cout<<"<!doctype html><html><head><meta charset='utf-8'><title>Bid or Sell</title>"
 "<style>body{font-family:system-ui;margin:2rem}form{border:1px solid #ddd;padding:1rem;margin:1rem 0;border-radius:10px}"
 "label{display:block;margin:.4rem 0}select,input,button,textarea{padding:.5rem;max-width:480px;width:100%}</style>"
 "</head><body>"; }
void footer(){ cout<<"</body></html>"; }

int main(){
    sqlite3* db=nullptr; sqlite3_open(DB_PATH,&db);
    // identify user
    string sid = getCookie(COOKIE_NAME);
    int uid=-1; string email;
    if(!sid.empty()){
        sqlite3_stmt* st=nullptr;
        sqlite3_prepare_v2(db,"SELECT u.id,u.email FROM sessions s JOIN users u ON u.id=s.user_id WHERE s.id=? AND s.expires_at>datetime('now');",-1,&st,nullptr);
        sqlite3_bind_text(st,1,sid.c_str(),-1,SQLITE_TRANSIENT);
        if(sqlite3_step(st)==SQLITE_ROW){ uid=sqlite3_column_int(st,0); email = (const char*)sqlite3_column_text(st,1); }
        sqlite3_finalize(st);
    }
    if(uid<0){ header(); cout<<"<p>Please <a href='/cgi-bin/auth.cgi'>log in</a>.</p>"; footer(); sqlite3_close(db); return 0; }

    string method = getenv("REQUEST_METHOD")? getenv("REQUEST_METHOD"): "GET";

    if(method=="GET"){
        header();
        cout<<"<h1>Bid or Sell</h1>";
        cout<<"<p>Signed in as <b>"<<htmlEscape(email)<<"</b></p>";

        // preselect?
        string qs = getenv("QUERY_STRING")? getenv("QUERY_STRING"): "";
        map<string,string> q = parseURLEncoded(qs);
        string pre = q.count("prefill_auction_id")? q["prefill_auction_id"]:"";

        // Bid form (dropdown of open auctions not owned by me)
        cout<<"<form method='POST'><h2>Bid on an Item</h2>"
              "<input type='hidden' name='action' value='bid'>"
              "<label>Item:</label><select name='auction_id' required>";
        {
            sqlite3_stmt* st=nullptr;
            sqlite3_prepare_v2(db,
               "SELECT a.id,a.title FROM auctions a "
               "WHERE datetime('now') BETWEEN a.start_time AND a.end_time "
               "AND a.seller_id != ? ORDER BY a.end_time ASC;",-1,&st,nullptr);
            sqlite3_bind_int(st,1,uid);
            while(sqlite3_step(st)==SQLITE_ROW){
                int id=sqlite3_column_int(st,0);
                const char* title=(const char*)sqlite3_column_text(st,1);
                string sel = (pre==to_string(id)? " selected":"");
                cout<<"<option value='"<<id<<"'"<<sel<<">"<<htmlEscape(title?title:"(untitled)")<<"</option>";
            }
            sqlite3_finalize(st);
        }
        cout<<"</select>"
              "<label>Your maximum bid amount</label>"
              "<input type='number' step='0.01' min='0.01' name='amount' required>"
              "<button type='submit'>Place/Update Max Bid</button></form>";

        // Sell form
        cout<<"<form method='POST'><h2>Sell an Item</h2>"
              "<input type='hidden' name='action' value='sell'>"
              "<label>Title</label><input name='title' required>"
              "<label>Description</label><textarea name='description' rows='4' required></textarea>"
              "<label>Starting price</label><input type='number' step='0.01' min='0' name='start_price' required>"
              "<label>Starting date/time (UTC, ISO 8601 e.g. 2025-09-01T12:00:00Z)</label>"
              "<input name='start_time' placeholder='YYYY-MM-DDThh:mm:ssZ' required>"
              "<p>Note: auction ends automatically 168 hours (7 days) after start time.</p>"
              "<button type='submit'>Create Auction</button></form>";

        cout<<"<p><a href='/cgi-bin/list_open.cgi'>View all open auctions</a> • <a href='/cgi-bin/transactions.cgi'>Your transactions</a></p>";
        footer(); sqlite3_close(db); return 0;
    }

    // POST
    size_t clen = (size_t)(getenv("CONTENT_LENGTH")? atoi(getenv("CONTENT_LENGTH")):0);
    string body = readStdin(clen);
    auto f = parseURLEncoded(body);
    string action = f["action"];

    header();
    if(action=="bid"){
        int aid = stoi(f["auction_id"]);
        double amt = stod(f["amount"]);
        // validate auction is open and not mine
        sqlite3_stmt* st=nullptr;
        sqlite3_prepare_v2(db,"SELECT seller_id, (datetime('now') BETWEEN start_time AND end_time) AS is_open FROM auctions WHERE id=?;",-1,&st,nullptr);
        sqlite3_bind_int(st,1,aid);
        int seller=-1, is_open=0;
        if(sqlite3_step(st)==SQLITE_ROW){ seller=sqlite3_column_int(st,0); is_open=sqlite3_column_int(st,1); }
        sqlite3_finalize(st);
        if(seller==-1){ cout<<"<p>Invalid auction.</p>"; footer(); sqlite3_close(db); return 0; }
        if(seller==uid){ cout<<"<p>You cannot bid on your own item.</p>"; footer(); sqlite3_close(db); return 0; }
        if(!is_open){ cout<<"<p>Auction not open.</p>"; footer(); sqlite3_close(db); return 0; }

        // upsert unique(auction_id,bidder_id). If exists and amt > current, update; else insert.
        sqlite3_stmt* stget=nullptr;
        sqlite3_prepare_v2(db,"SELECT max_bid FROM bids WHERE auction_id=? AND bidder_id=?;",-1,&stget,nullptr);
        sqlite3_bind_int(stget,1,aid); sqlite3_bind_int(stget,2,uid);
        bool have=false; double cur=0;
        if(sqlite3_step(stget)==SQLITE_ROW){ have=true; cur=sqlite3_column_double(stget,0); }
        sqlite3_finalize(stget);
        if(have){
            if(amt>cur){
                sqlite3_stmt* st2=nullptr;
                sqlite3_prepare_v2(db,"UPDATE bids SET max_bid=?, created_at=datetime('now') WHERE auction_id=? AND bidder_id=?;",-1,&st2,nullptr);
                sqlite3_bind_double(st2,1,amt); sqlite3_bind_int(st2,2,aid); sqlite3_bind_int(st2,3,uid);
                sqlite3_step(st2); sqlite3_finalize(st2);
                cout<<"<p>Max bid increased to <b>"<<fixed<<setprecision(2)<<amt<<"</b>.</p>";
            } else {
                cout<<"<p>Your existing max bid (<b>"<<fixed<<setprecision(2)<<cur<<"</b>) is already ≥ submitted amount.</p>";
            }
        } else {
            sqlite3_stmt* sti=nullptr;
            sqlite3_prepare_v2(db,"INSERT INTO bids(auction_id,bidder_id,max_bid,created_at) VALUES(?,?,?,datetime('now'));",-1,&sti,nullptr);
            sqlite3_bind_int(sti,1,aid); sqlite3_bind_int(sti,2,uid); sqlite3_bind_double(sti,3,amt);
            sqlite3_step(sti); sqlite3_finalize(sti);
            cout<<"<p>Max bid set to <b>"<<fixed<<setprecision(2)<<amt<<"</b>.</p>";
        }
        cout<<"<p><a href='/cgi-bin/transactions.cgi'>Your transactions</a> • <a href='/cgi-bin/list_open.cgi'>Open auctions</a></p>";
    } else if(action=="sell"){
        string title=f["title"], desc=f["description"], start=f["start_time"];
        double sp = stod(f["start_price"]);
        if(title.empty()||desc.empty()||start.empty()){ cout<<"<p>All fields required.</p>"; footer(); sqlite3_close(db); return 0; }
        // create auction; end_time = start + 168 hours using SQLite
        sqlite3_stmt* st=nullptr;
        sqlite3_prepare_v2(db,
            "INSERT INTO auctions(seller_id,title,description,start_price,start_time,end_time,created_at) "
            "VALUES(?,?,?,?,?,datetime(?,'+168 hours'),datetime('now'));",-1,&st,nullptr);
        sqlite3_bind_int(st,1,uid);
        sqlite3_bind_text(st,2,title.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(st,3,desc.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_double(st,4,sp);
        sqlite3_bind_text(st,5,start.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(st,6,start.c_str(),-1,SQLITE_TRANSIENT);
        if(sqlite3_step(st)==SQLITE_DONE){
            cout<<"<p>Auction created: <b>"<<htmlEscape(title)<<"</b></p>";
        } else {
            cout<<"<p>Failed to create auction. Ensure start time is ISO 8601 UTC (e.g., 2025-09-01T12:00:00Z).</p>";
        }
        sqlite3_finalize(st);
        cout<<"<p><a href='/cgi-bin/list_open.cgi'>Open auctions</a> • <a href='/cgi-bin/transactions.cgi'>Your transactions</a></p>";
    } else {
        cout<<"<p>Unknown action.</p>";
    }
    footer();
    sqlite3_close(db);
    return 0;
}
