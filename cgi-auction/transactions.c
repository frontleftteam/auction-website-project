/* transactions.c  (gcc -O2 transactions.c -lsqlite3 -o transactions.cgi) */
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* DB_PATH = "auction.db";
static const char* COOKIE_NAME = "AUCTSESS";

void html_escape(const char* in, char* out, size_t outsz){
    size_t j=0;
    for(size_t i=0; in[i] && j+6<outsz; ++i){
        char c=in[i];
        if(c=='&'){ j+=snprintf(out+j,outsz-j,"&amp;"); }
        else if(c=='<'){ j+=snprintf(out+j,outsz-j,"&lt;"); }
        else if(c=='>'){ j+=snprintf(out+j,outsz-j,"&gt;"); }
        else if(c=='"'){ j+=snprintf(out+j,outsz-j,"&quot;"); }
        else if(c=='\''){ j+=snprintf(out+j,outsz-j,"&#39;"); }
        else { out[j++]=c; }
    }
    out[j]=0;
}

char* get_cookie_value(const char* name){
    const char* cookies = getenv("HTTP_COOKIE");
    if(!cookies) return NULL;
    size_t nlen = strlen(name);
    const char* p=cookies;
    while(p && *p){
        while(*p==' ') p++;
        if(!strncmp(p,name,nlen) && p[nlen]=='='){
            p += nlen+1;
            const char* e = strchr(p,';');
            size_t len = e? (size_t)(e-p): strlen(p);
            char* val = (char*)malloc(len+1);
            memcpy(val,p,len); val[len]=0;
            return val;
        }
        p = strchr(p,';');
        if(p) p++;
    }
    return NULL;
}

void print_header(){
    printf("Content-Type: text/html\r\n\r\n");
    printf("<!doctype html><html><head><meta charset='utf-8'>"
           "<title>My Transactions</title>"
           "<style>body{font-family:system-ui,Arial;margin:2rem} h2{margin-top:1.5rem}"
           "table{border-collapse:collapse;margin:.5rem 0} td,th{border:1px solid #ddd;padding:.4rem .6rem}"
           ".warn{color:#b00}</style></head><body>");
}
void print_footer(){ printf("</body></html>"); }

int main(){
    sqlite3* db=NULL;
    if(sqlite3_open(DB_PATH,&db)!=SQLITE_OK){
        print_header();
        printf("<p>DB error.</p>");
        print_footer();
        return 0;
    }
    // who am i?
    int user_id=-1; char* email=NULL;
    char* sid = get_cookie_value(COOKIE_NAME);
    if(sid){
        sqlite3_stmt* st=NULL;
        sqlite3_prepare_v2(db,
            "SELECT u.id,u.email FROM sessions s JOIN users u ON u.id=s.user_id "
            "WHERE s.id=? AND s.expires_at>datetime('now');",-1,&st,NULL);
        sqlite3_bind_text(st,1,sid,-1,SQLITE_TRANSIENT);
        if(sqlite3_step(st)==SQLITE_ROW){
            user_id = sqlite3_column_int(st,0);
            const unsigned char* em = sqlite3_column_text(st,1);
            if(em){ email=strdup((const char*)em); }
        }
        sqlite3_finalize(st);
    }
    print_header();
    if(user_id<0){
        printf("<p>Please <a href='/cgi-bin/auth.cgi'>log in</a>.</p>");
        print_footer();
        sqlite3_close(db);
        if(sid) free(sid);
        return 0;
    }
    printf("<h1>Your Transactions</h1>");
    printf("<p>Signed in as <b>%s</b>. <a href='/cgi-bin/list_open.cgi'>Browse open auctions</a></p>", email?email:"");

    // 1) Selling (open then sold)
    printf("<h2>1. Selling</h2><h3>Active</h3>");
    {
        sqlite3_stmt* st=NULL;
        sqlite3_prepare_v2(db,
           "SELECT id,title,description,start_price,start_time,end_time "
           "FROM auctions WHERE seller_id=? AND datetime('now')<end_time "
           "ORDER BY end_time ASC;",-1,&st,NULL);
        sqlite3_bind_int(st,1,user_id);
        printf("<table><tr><th>ID</th><th>Title</th><th>Ends</th><th>Current Highest</th></tr>");
        while(sqlite3_step(st)==SQLITE_ROW){
            int id=sqlite3_column_int(st,0);
            const char* title=(const char*)sqlite3_column_text(st,1);
            const char* end=(const char*)sqlite3_column_text(st,5);
            // current highest
            sqlite3_stmt* st2=NULL;
            sqlite3_prepare_v2(db,"SELECT MAX(max_bid) FROM bids WHERE auction_id=?;",-1,&st2,NULL);
            sqlite3_bind_int(st2,1,id);
            double cur=0; if(sqlite3_step(st2)==SQLITE_ROW){ if(sqlite3_column_type(st2,0)!=SQLITE_NULL) cur=sqlite3_column_double(st2,0);}
            sqlite3_finalize(st2);
            char esc[512]; html_escape(title?title:"",esc,sizeof esc);
            printf("<tr><td>%d</td><td>%s</td><td>%s</td><td>%.2f</td></tr>",id,esc,end?end:"",cur);
        }
        printf("</table>");
        sqlite3_finalize(st);
        // Sold (closed + had a winning bid)
        printf("<h3>Sold (Closed)</h3>");
        sqlite3_prepare_v2(db,
           "SELECT a.id,a.title,MAX(b.max_bid) AS win "
           "FROM auctions a JOIN bids b ON b.auction_id=a.id "
           "WHERE a.seller_id=? AND datetime('now')>=a.end_time "
           "GROUP BY a.id,a.title ORDER BY a.end_time DESC;",-1,&st,NULL);
        sqlite3_bind_int(st,1,user_id);
        printf("<table><tr><th>ID</th><th>Title</th><th>Winning Price</th></tr>");
        while(sqlite3_step(st)==SQLITE_ROW){
            int id=sqlite3_column_int(st,0);
            const char* title=(const char*)sqlite3_column_text(st,1);
            double win=sqlite3_column_double(st,2);
            char esc[512]; html_escape(title?title:"",esc,sizeof esc);
            printf("<tr><td>%d</td><td>%s</td><td>%.2f</td></tr>",id,esc,win);
        }
        printf("</table>");
        sqlite3_finalize(st);
    }

    // 2) Purchases (closed where I'm top bidder)
    printf("<h2>2. Purchases</h2>");
    {
        sqlite3_stmt* st=NULL;
        sqlite3_prepare_v2(db,
           "SELECT a.id,a.title, (SELECT MAX(b2.max_bid) FROM bids b2 WHERE b2.auction_id=a.id) AS price "
           "FROM auctions a "
           "WHERE datetime('now')>=a.end_time "
           "AND EXISTS ("
           "  SELECT 1 FROM bids bx "
           "  WHERE bx.auction_id=a.id AND bx.bidder_id=? "
           "  AND bx.max_bid = (SELECT MAX(b2.max_bid) FROM bids b2 WHERE b2.auction_id=a.id)"
           ") "
           "ORDER BY a.end_time DESC;",-1,&st,NULL);
        sqlite3_bind_int(st,1,user_id);
        printf("<table><tr><th>ID</th><th>Title</th><th>Price</th></tr>");
        while(sqlite3_step(st)==SQLITE_ROW){
            int id=sqlite3_column_int(st,0);
            const char* title=(const char*)sqlite3_column_text(st,1);
            double price=sqlite3_column_double(st,2);
            char esc[512]; html_escape(title?title:"",esc,sizeof esc);
            printf("<tr><td>%d</td><td>%s</td><td>%.2f</td></tr>",id,esc,price);
        }
        printf("</table>");
        sqlite3_finalize(st);
    }

    // 3) Current Bids (open auctions I bid on). Outbid warning + "Increase" link
    printf("<h2>3. Current Bids</h2>");
    {
        sqlite3_stmt* st=NULL;
        sqlite3_prepare_v2(db,
           "SELECT a.id,a.title,"
           " (SELECT MAX(b2.max_bid) FROM bids b2 WHERE b2.auction_id=a.id) AS cur,"
           " (SELECT CASE WHEN EXISTS ("
           "     SELECT 1 FROM bids b3 WHERE b3.auction_id=a.id AND b3.bidder_id=? "
           "     AND b3.max_bid = (SELECT MAX(b4.max_bid) FROM bids b4 WHERE b4.auction_id=a.id)"
           " ) THEN 1 ELSE 0 END) AS is_top "
           "FROM auctions a "
           "WHERE datetime('now') BETWEEN a.start_time AND a.end_time "
           "AND EXISTS(SELECT 1 FROM bids bu WHERE bu.auction_id=a.id AND bu.bidder_id=?) "
           "ORDER BY a.end_time ASC;",-1,&st,NULL);
        sqlite3_bind_int(st,1,user_id);
        sqlite3_bind_int(st,2,user_id);
        printf("<table><tr><th>ID</th><th>Title</th><th>Highest</th><th>Status</th><th>Action</th></tr>");
        while(sqlite3_step(st)==SQLITE_ROW){
            int id=sqlite3_column_int(st,0);
            const char* title=(const char*)sqlite3_column_text(st,1);
            double cur= sqlite3_column_type(st,2)==SQLITE_NULL? 0.0: sqlite3_column_double(st,2);
            int is_top = sqlite3_column_int(st,3);
            char esc[512]; html_escape(title?title:"",esc,sizeof esc);
            printf("<tr><td>%d</td><td>%s</td><td>%.2f</td><td>%s</td>"
                   "<td><a href='/cgi-bin/bid_sell.cgi?prefill_auction_id=%d'>Increase Max Bid</a></td></tr>",
                   id,esc,cur,is_top?"You're leading":"<span class=\"warn\">Outbid</span>", id);
        }
        printf("</table>");
        sqlite3_finalize(st);
    }

    // 4) Didn't Win (closed auctions I bid on, but not top)
    printf("<h2>4. Didn’t Win</h2>");
    {
        sqlite3_stmt* st=NULL;
        sqlite3_prepare_v2(db,
           "SELECT a.id,a.title,"
           " (SELECT MAX(b2.max_bid) FROM bids b2 WHERE b2.auction_id=a.id) AS win "
           "FROM auctions a "
           "WHERE datetime('now')>=a.end_time "
           "AND EXISTS(SELECT 1 FROM bids bu WHERE bu.auction_id=a.id AND bu.bidder_id=?) "
           "AND (SELECT bidder_id FROM bids WHERE auction_id=a.id ORDER BY max_bid DESC, id ASC LIMIT 1) != ? "
           "ORDER BY a.end_time DESC;",-1,&st,NULL);
        sqlite3_bind_int(st,1,user_id);
        sqlite3_bind_int(st,2,user_id);
        printf("<table><tr><th>ID</th><th>Title</th><th>Winning Price</th></tr>");
        while(sqlite3_step(st)==SQLITE_ROW){
            int id=sqlite3_column_int(st,0);
            const char* title=(const char*)sqlite3_column_text(st,1);
            double win= sqlite3_column_type(st,2)==SQLITE_NULL? 0.0: sqlite3_column_double(st,2);
            char esc[512]; html_escape(title?title:"",esc,sizeof esc);
            printf("<tr><td>%d</td><td>%s</td><td>%.2f</td></tr>",id,esc,win);
        }
        printf("</table>");
        sqlite3_finalize(st);
    }

    print_footer();
    sqlite3_close(db);
    if(sid) free(sid);
    if(email) free(email);
    return 0;
}
