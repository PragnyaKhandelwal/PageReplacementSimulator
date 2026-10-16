#include "io/ReportWriter.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "algorithms/AlgorithmFactory.h"
#include "core/Simulator.h"
#include "core/Util.h"

namespace prs {

namespace {

std::ofstream openOut(const std::string& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write '" + path + "'");
    return out;
}

std::string timestamp() {
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", std::localtime(&now));
    return buf;
}

// Round up to 1, 2, 2.5 or 5 x 10^k so axis ticks are clean.
double niceMax(double v) {
    if (v <= 0) return 1;
    double exp = std::pow(10.0, std::floor(std::log10(v)));
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0})
        if (m * exp >= v) return m * exp;
    return 10 * exp;
}

std::string num(double v) { return fixed(v, 1); }

std::string tick(double v) {
    if (std::fabs(v - std::round(v)) < 1e-9) {
        long long iv = static_cast<long long>(std::llround(v));
        if (iv >= 1000000) return fixed(iv / 1e6, 1) + "M";
        if (iv >= 10000) return fixed(iv / 1e3, 1) + "K";
        return std::to_string(iv);
    }
    return fixed(v, 1);
}

// Horizontal bar with a 4px rounded data end and a square baseline.
std::string hBar(double x, double y, double w, double h, const std::string& cls, const std::string& tip) {
    std::ostringstream s;
    double r = std::min(4.0, std::min(w, h / 2));
    s << "<path class=\"" << cls << "\" data-tip=\"" << htmlEscape(tip) << "\" d=\"M" << num(x) << ' ' << num(y)
      << " h" << num(w - r) << " a" << num(r) << ' ' << num(r) << " 0 0 1 " << num(r) << ' ' << num(r)
      << " v" << num(h - 2 * r) << " a" << num(r) << ' ' << num(r) << " 0 0 1 " << num(-r) << ' ' << num(r)
      << " h" << num(-(w - r)) << " z\"/>";
    return s.str();
}

// Vertical column with a rounded top.
std::string vBar(double x, double base, double w, double h, const std::string& cls, const std::string& tip) {
    std::ostringstream s;
    double r = std::min(4.0, std::min(h, w / 2));
    s << "<path class=\"" << cls << "\" data-tip=\"" << htmlEscape(tip) << "\" d=\"M" << num(x) << ' ' << num(base)
      << " v" << num(-(h - r)) << " a" << num(r) << ' ' << num(r) << " 0 0 1 " << num(r) << ' ' << num(-r)
      << " h" << num(w - 2 * r) << " a" << num(r) << ' ' << num(r) << " 0 0 1 " << num(r) << ' ' << num(r)
      << " v" << num(h - r) << " z\"/>";
    return s.str();
}

const char* CSS = R"CSS(
:root{color-scheme:light;--page:#f9f9f7;--surface:#fcfcfb;--ink:#0b0b0b;--ink-2:#52514e;--muted:#898781;
--grid:#e1e0d9;--axis:#c3c2b7;--border:rgba(11,11,11,.10);--series-1:#2a78d6;--good:#0ca30c;--good-text:#006300;
--critical:#d03b3b;--hit-bg:rgba(12,163,12,.13);--fault-bg:rgba(208,59,59,.13);
--h1:#cde2fb;--h2:#9ec5f4;--h3:#6da7ec;--h4:#3987e5;--h5:#256abf;--h6:#184f95;--h7:#0d366b}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){color-scheme:dark;--page:#0d0d0d;--surface:#1a1a19;
--ink:#f0efec;--ink-2:#c3c2b7;--muted:#898781;--grid:#2c2c2a;--axis:#383835;--border:rgba(255,255,255,.10);
--series-1:#3987e5;--good-text:#0ca30c;--hit-bg:rgba(12,163,12,.22);--fault-bg:rgba(208,59,59,.25);
--h1:#0d366b;--h2:#104281;--h3:#184f95;--h4:#1c5cab;--h5:#2a78d6;--h6:#5598e7;--h7:#86b6ef}}
:root[data-theme="dark"]{color-scheme:dark;--page:#0d0d0d;--surface:#1a1a19;--ink:#f0efec;--ink-2:#c3c2b7;
--muted:#898781;--grid:#2c2c2a;--axis:#383835;--border:rgba(255,255,255,.10);--series-1:#3987e5;--good-text:#0ca30c;
--hit-bg:rgba(12,163,12,.22);--fault-bg:rgba(208,59,59,.25);
--h1:#0d366b;--h2:#104281;--h3:#184f95;--h4:#1c5cab;--h5:#2a78d6;--h6:#5598e7;--h7:#86b6ef}
*{box-sizing:border-box}
body{margin:0;background:var(--page);color:var(--ink);font:15px/1.55 system-ui,-apple-system,"Segoe UI",sans-serif}
main{max-width:1080px;margin:0 auto;padding:32px 16px 64px}
header{display:flex;justify-content:space-between;gap:16px;align-items:flex-start;flex-wrap:wrap}
h1{font-size:28px;margin:0 0 4px;letter-spacing:-.01em}
h2{font-size:19px;margin:0 0 4px}
.sub{color:var(--ink-2);margin:0}
.muted{color:var(--muted);font-size:13px}
section{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:20px;margin-top:20px}
section>p.lead{color:var(--ink-2);margin:0 0 14px}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;margin-top:20px}
.tile{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:14px 16px}
.tile .label{color:var(--ink-2);font-size:13px}
.tile .value{font-size:26px;font-weight:600;margin-top:2px}
.tile .note{color:var(--muted);font-size:12px}
.scroll{overflow-x:auto}
table{border-collapse:collapse;width:100%;font-size:14px}
th,td{padding:7px 10px;text-align:right;border-bottom:1px solid var(--grid);white-space:nowrap}
th{color:var(--ink-2);font-weight:600;font-size:13px}
th:first-child,td:first-child,th.l,td.l{text-align:left}
td{font-variant-numeric:tabular-nums}
tr.best td{font-weight:600}
.pill{display:inline-block;padding:1px 8px;border-radius:99px;font-size:12px;border:1px solid var(--border);color:var(--ink-2)}
.pill.good{color:var(--good-text)} .pill.bad{color:var(--critical)}
svg{display:block;width:100%;height:auto}
svg text{fill:var(--muted);font-size:11px;font-family:inherit}
svg text.ink{fill:var(--ink-2);font-size:12px}
svg text.title{fill:var(--ink);font-size:13px;font-weight:600}
.gridline{stroke:var(--grid);stroke-width:1}
.baseline{stroke:var(--axis);stroke-width:1}
.bar{fill:var(--series-1)} .bar.ref{fill:var(--axis)}
.line{fill:none;stroke:var(--series-1);stroke-width:2;stroke-linejoin:round;stroke-linecap:round}
.refline{fill:none;stroke:var(--muted);stroke-width:1.5;stroke-linejoin:round}
.area{fill:var(--series-1);opacity:.10}
.dot{fill:var(--series-1);stroke:var(--surface);stroke-width:2}
.dot.bad{fill:var(--critical)}
.hit-target{fill:transparent}
.h0{fill:transparent}.h1{fill:var(--h1)}.h2{fill:var(--h2)}.h3{fill:var(--h3)}.h4{fill:var(--h4)}.h5{fill:var(--h5)}.h6{fill:var(--h6)}.h7{fill:var(--h7)}
.multiples{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:12px}
.panel{border:1px solid var(--border);border-radius:10px;padding:6px}
.legend{display:flex;gap:16px;flex-wrap:wrap;color:var(--ink-2);font-size:13px;margin:6px 0 10px}
.key{display:inline-block;width:14px;height:3px;border-radius:2px;vertical-align:middle;margin-right:6px}
.sw{display:inline-block;width:10px;height:10px;border-radius:3px;vertical-align:-1px;margin-right:6px}
ul.insights{margin:10px 0 0;padding-left:20px} ul.insights li{margin:4px 0}
.kv{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:6px 20px;font-size:14px}
.kv div span{color:var(--ink-2)}
table.steps{font-size:13px;width:auto}
table.steps th,table.steps td{text-align:center;padding:4px 7px;min-width:30px;border-bottom:1px solid var(--grid)}
table.steps th:first-child,table.steps td:first-child{text-align:left;color:var(--ink-2);position:sticky;left:0;background:var(--surface)}
td.hit{background:var(--hit-bg);font-weight:600} td.fault{background:var(--fault-bg);font-weight:600}
td.res-h{color:var(--good-text);font-weight:600} td.res-f{color:var(--critical);font-weight:600}
details{border-top:1px solid var(--grid);padding:10px 0}
details summary{cursor:pointer;font-weight:600}
details summary span{color:var(--ink-2);font-weight:400}
ol.log{font-size:13px;color:var(--ink-2);columns:2 420px;margin:8px 0 0;padding-left:28px}
button.theme{background:var(--surface);color:var(--ink);border:1px solid var(--border);border-radius:8px;padding:6px 12px;cursor:pointer;font:inherit;font-size:13px}
#tip{position:fixed;pointer-events:none;background:var(--ink);color:var(--surface);padding:6px 9px;border-radius:6px;font-size:12px;max-width:280px;z-index:9;box-shadow:0 2px 8px rgba(0,0,0,.2)}
footer{margin-top:28px;color:var(--muted);font-size:13px;text-align:center}
.rp-controls{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:12px}
.rp-controls select,.rp-controls button{background:var(--surface);color:var(--ink);border:1px solid var(--border);border-radius:8px;padding:6px 12px;font:inherit;font-size:13px;cursor:pointer}
.rp-controls button.primary{background:var(--series-1);color:#fff;border-color:transparent;min-width:76px}
.rp-controls input[type=range]{flex:1;min-width:140px}
#rp-refs{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:14px}
.chip{min-width:30px;text-align:center;padding:3px 6px;border-radius:6px;border:1px solid var(--border);font-size:13px;font-variant-numeric:tabular-nums;cursor:pointer;color:var(--ink-2)}
.chip.h{background:var(--hit-bg)} .chip.f{background:var(--fault-bg)}
.chip.cur{outline:2px solid var(--series-1);font-weight:700;color:var(--ink)}
#rp-frames{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:12px}
.fr{width:84px;border:1px solid var(--border);border-radius:10px;padding:8px;text-align:center;transition:background .25s}
.fr .fl{display:block;font-size:11px;color:var(--muted)}
.fr b{display:block;font-size:24px;min-height:34px;line-height:34px}
.fr .ev{display:block;font-size:11px;color:var(--critical);min-height:14px}
.fr.hit{background:var(--hit-bg)} .fr.fault{background:var(--fault-bg)}
#rp-status{min-height:44px;color:var(--ink-2)}
#rp-counts{font-weight:600}
)CSS";

const char* SCRIPT = R"JS(
const tip=document.getElementById('tip');
document.addEventListener('mouseover',e=>{const t=e.target.closest('[data-tip]');
 if(!t){tip.hidden=true;return;} tip.textContent=t.getAttribute('data-tip'); tip.hidden=false;});
document.addEventListener('mousemove',e=>{if(tip.hidden)return;let x=e.clientX+14,y=e.clientY+14;
 const r=tip.getBoundingClientRect(); if(x+r.width>innerWidth-8)x=e.clientX-r.width-14;
 if(y+r.height>innerHeight-8)y=e.clientY-r.height-14; tip.style.left=x+'px'; tip.style.top=y+'px';});
document.getElementById('theme').addEventListener('click',()=>{const d=document.documentElement;
 const dark=d.dataset.theme?d.dataset.theme==='dark':matchMedia('(prefers-color-scheme: dark)').matches;
 d.dataset.theme=dark?'light':'dark';});
(function(){
 const el=document.getElementById('replay-data'); if(!el) return;
 const D=JSON.parse(el.textContent), $=id=>document.getElementById(id);
 const sel=$('rp-algo'), slider=$('rp-slider'), refsBox=$('rp-refs'), framesBox=$('rp-frames');
 const status=$('rp-status'), counts=$('rp-counts'), play=$('rp-play'), speed=$('rp-speed');
 let ai=0, t=-1, timer=null;
 D.algos.forEach((a,i)=>{const o=document.createElement('option'); o.value=i; o.textContent=a.key+' — '+a.name; sel.appendChild(o);});
 slider.max=D.refs.length;
 D.refs.forEach((r,i)=>{const s=document.createElement('span'); s.className='chip'; s.textContent=r.p+(r.w?'w':'');
  s.onclick=()=>go(i); refsBox.appendChild(s);});
 function buildFrames(){framesBox.innerHTML=''; for(let k=0;k<D.algos[ai].frames;k++){
  const d=document.createElement('div'); d.className='fr';
  d.innerHTML='<span class="fl">Frame '+k+'</span><b></b><span class="ev"></span>'; framesBox.appendChild(d);}}
 function render(){
  const a=D.algos[ai]; let hits=0, faults=0, wb=0;
  for(let i=0;i<=t;i++){const s=a.s[i]; if(s[2])hits++; else faults++; if(s[4])wb++;}
  Array.from(refsBox.children).forEach((c,i)=>{c.className='chip'+(i<=t?(a.s[i][2]?' h':' f'):'')+(i===t?' cur':'');});
  const s=t>=0?a.s[t]:null;
  Array.from(framesBox.children).forEach((d,k)=>{
   const page=s?s[0][k]:-1;
   d.className='fr'+(s&&s[1]===k?(s[2]?' hit':' fault'):'');
   d.children[1].textContent=page<0?'':page;
   d.children[2].textContent=(s&&s[1]===k&&!s[2]&&s[3]>=0)?'evicted '+s[3]+(s[4]?' (dirty)':''):'';});
  status.textContent=s?('ref '+D.refs[t].p+(D.refs[t].w?' (write)':'')+': '+(s[2]?'HIT':'FAULT')+' — '+s[5]):'Press Play or Step to begin.';
  counts.textContent=faults+' faults · '+hits+' hits'+(wb?' · '+wb+' write-backs':'');
  slider.value=t+1;
 }
 function go(i){t=Math.max(-1,Math.min(D.refs.length-1,i)); render();}
 function stop(){if(timer){clearInterval(timer); timer=null;} play.textContent='Play';}
 function start(){if(t>=D.refs.length-1)t=-1; play.textContent='Pause';
  timer=setInterval(()=>{if(t>=D.refs.length-1){stop();return;} go(t+1);},1000/parseFloat(speed.value));}
 play.onclick=()=>timer?stop():start();
 speed.onchange=()=>{if(timer){stop(); start();}};
 $('rp-step').onclick=()=>{stop(); go(t+1);};
 $('rp-back').onclick=()=>{stop(); go(t-1);};
 $('rp-reset').onclick=()=>{stop(); go(-1);};
 slider.oninput=()=>{stop(); go(parseInt(slider.value,10)-1);};
 sel.onchange=()=>{ai=parseInt(sel.value,10); buildFrames(); render();};
 buildFrames(); render();
})();
)JS";

// ----------------------------------------------------------- sections ----

void faultBarChart(std::ostream& o, const ReportBundle& b) {
    auto ranked = rankResults(b.results, b.optFaults);
    const double W = 760, left = 80, right = 150, rowH = 32, barH = 18, top = 8;
    const double H = top + rowH * ranked.size() + 26;
    long long maxF = 1;
    for (const auto& r : b.results) maxF = std::max(maxF, r.faults);
    const double vmax = niceMax(static_cast<double>(maxF));
    const double plotW = W - left - right;

    o << "<svg viewBox=\"0 0 " << W << ' ' << num(H) << "\" role=\"img\" aria-label=\"Page faults by policy\">";
    for (int t = 0; t <= 4; ++t) {
        double x = left + plotW * t / 4;
        o << "<line class=\"gridline\" x1=\"" << num(x) << "\" y1=\"" << top << "\" x2=\"" << num(x)
          << "\" y2=\"" << num(H - 22) << "\"/>";
        o << "<text x=\"" << num(x) << "\" y=\"" << num(H - 6) << "\" text-anchor=\"middle\">"
          << tick(vmax * t / 4) << "</text>";
    }
    for (size_t i = 0; i < ranked.size(); ++i) {
        const SimulationResult& r = *ranked[i].result;
        double y = top + i * rowH + (rowH - barH) / 2;
        double w = std::max(1.0, plotW * r.faults / vmax);
        o << "<text class=\"ink\" x=\"" << left - 10 << "\" y=\"" << num(y + barH / 2 + 4)
          << "\" text-anchor=\"end\">" << r.key << "</text>";
        std::string tip = cat(r.algorithm, ": ", r.faults, " faults, ", r.hits, " hits (fault ratio ",
                              percent(r.faultRatio()), ")");
        o << hBar(left, y, w, barH, r.key == "OPT" ? "bar ref" : "bar", tip);
        o << "<text class=\"ink\" x=\"" << num(left + w + 8) << "\" y=\"" << num(y + barH / 2 + 4) << "\">"
          << r.faults << " faults &#183; " << percent(r.faultRatio(), 1) << "</text>";
    }
    o << "<line class=\"baseline\" x1=\"" << left << "\" y1=\"" << top << "\" x2=\"" << left << "\" y2=\""
      << num(H - 22) << "\"/></svg>";
    o << "<p class=\"muted\">Sorted best to worst. The gray bar is OPT, the theoretical lower bound.</p>";
}

void comparisonTable(std::ostream& o, const ReportBundle& b) {
    auto ranked = rankResults(b.results, b.optFaults);
    o << "<div class=\"scroll\"><table><thead><tr><th>#</th><th class=\"l\">Policy</th><th>Faults</th>"
         "<th>Hits</th><th>Hit ratio</th><th>Fault ratio</th><th>Compulsory</th><th>Capacity</th>"
         "<th>Write-backs</th><th>EAT</th><th>vs OPT</th><th>Optimality</th></tr></thead><tbody>";
    int bestRank = -1;
    for (const auto& rr : ranked)
        if (rr.result->key != "OPT") { bestRank = rr.rank; break; }
    for (const auto& rr : ranked) {
        const SimulationResult& r = *rr.result;
        bool best = r.key != "OPT" && rr.rank == bestRank;
        o << "<tr" << (best ? " class=\"best\"" : "") << "><td>" << rr.rank << "</td><td class=\"l\">"
          << "<b>" << r.key << "</b> <span class=\"muted\">" << htmlEscape(r.algorithm) << "</span>"
          << (best ? " <span class=\"pill good\">best practical</span>" : "")
          << (r.key == "OPT" ? " <span class=\"pill\">benchmark</span>" : "") << "</td><td>" << r.faults
          << "</td><td>" << r.hits << "</td><td>" << percent(r.hitRatio()) << "</td><td>"
          << percent(r.faultRatio()) << "</td><td>" << r.compulsoryFaults << "</td><td>" << r.capacityFaults
          << "</td><td>" << r.writeBacks << "</td><td>" << formatNs(r.effectiveAccessNs) << "</td><td>"
          << (rr.extraFaultsVsOpt ? "+" + std::to_string(rr.extraFaultsVsOpt) : std::string("="))
          << "</td><td>" << fixed(rr.optimalityPercent, 1) << "%</td></tr>";
    }
    o << "</tbody></table></div>";
}

void sweepMultiples(std::ostream& o, const ReportBundle& b) {
    const SweepResult& s = b.sweep;
    if (s.frameCounts.size() < 2) return;
    long long maxF = 1;
    for (const auto& row : s.faults) for (long long v : row) maxF = std::max(maxF, v);
    const double vmax = niceMax(static_cast<double>(maxF));
    const std::vector<long long>* opt = nullptr;
    for (size_t a = 0; a < s.algorithms.size(); ++a)
        if (s.algorithms[a] == "OPT") opt = &s.faults[a];

    const double W = 230, H = 150, l = 36, r = 12, t = 26, bt = 24;
    const double pw = W - l - r, ph = H - t - bt;
    auto X = [&](size_t i) { return l + pw * i / (s.frameCounts.size() - 1); };
    auto Y = [&](double v) { return t + ph * (1 - v / vmax); };

    o << "<div class=\"legend\"><span><i class=\"key\" style=\"background:var(--series-1)\"></i>policy</span>"
         "<span><i class=\"key\" style=\"background:var(--muted)\"></i>OPT (lower bound)</span>"
         "<span><i class=\"sw\" style=\"background:var(--critical);border-radius:50%\"></i>Belady's anomaly</span></div>";
    o << "<div class=\"multiples\">";
    for (size_t a = 0; a < s.algorithms.size(); ++a) {
        const auto& row = s.faults[a];
        o << "<div class=\"panel\"><svg viewBox=\"0 0 " << W << ' ' << H << "\" role=\"img\" aria-label=\""
          << s.algorithms[a] << " faults versus frames\">";
        o << "<text class=\"title\" x=\"" << l << "\" y=\"16\">" << s.algorithms[a] << "</text>";
        for (int k = 0; k <= 2; ++k) {
            double v = vmax * k / 2, y = Y(v);
            o << "<line class=\"" << (k == 0 ? "baseline" : "gridline") << "\" x1=\"" << l << "\" y1=\"" << num(y)
              << "\" x2=\"" << W - r << "\" y2=\"" << num(y) << "\"/><text x=\"" << l - 6 << "\" y=\"" << num(y + 4)
              << "\" text-anchor=\"end\">" << tick(v) << "</text>";
        }
        size_t n = s.frameCounts.size();
        for (size_t i : {size_t(0), n / 2, n - 1}) {
            o << "<text x=\"" << num(X(i)) << "\" y=\"" << H - 8 << "\" text-anchor=\"middle\">"
              << s.frameCounts[i] << (i == n - 1 ? " fr" : "") << "</text>";
        }
        auto polyline = [&](const std::vector<long long>& v, const char* cls) {
            o << "<polyline class=\"" << cls << "\" points=\"";
            for (size_t i = 0; i < v.size(); ++i) o << num(X(i)) << ',' << num(Y(static_cast<double>(v[i]))) << ' ';
            o << "\"/>";
        };
        if (opt && s.algorithms[a] != "OPT") polyline(*opt, "refline");
        polyline(row, "line");
        // End dot + anomaly markers + hover targets.
        o << "<circle class=\"dot\" r=\"4\" cx=\"" << num(X(n - 1)) << "\" cy=\"" << num(Y(row[n - 1])) << "\"/>";
        for (size_t i = 0; i < n; ++i) {
            bool anomaly = i > 0 && row[i] > row[i - 1];
            if (anomaly)
                o << "<circle class=\"dot bad\" r=\"4.5\" cx=\"" << num(X(i)) << "\" cy=\"" << num(Y(row[i])) << "\"/>";
            std::string tip = cat(s.algorithms[a], " with ", s.frameCounts[i], " frames: ", row[i], " faults");
            if (opt) tip += cat(" (OPT ", (*opt)[i], ")");
            if (anomaly) tip += cat(" - more than with ", s.frameCounts[i - 1], " frames!");
            o << "<circle class=\"hit-target\" r=\"9\" cx=\"" << num(X(i)) << "\" cy=\"" << num(Y(row[i]))
              << "\" data-tip=\"" << htmlEscape(tip) << "\"/>";
        }
        o << "</svg></div>";
    }
    o << "</div>";

    // Table view of the same data.
    o << "<details><summary>Table view <span>(faults per frame count)</span></summary><div class=\"scroll\"><table><thead><tr><th>Policy</th>";
    for (int f : s.frameCounts) o << "<th>" << f << "</th>";
    o << "</tr></thead><tbody>";
    for (size_t a = 0; a < s.algorithms.size(); ++a) {
        o << "<tr><td>" << s.algorithms[a] << "</td>";
        for (size_t i = 0; i < s.frameCounts.size(); ++i) {
            bool anomaly = i > 0 && s.faults[a][i] > s.faults[a][i - 1];
            o << "<td" << (anomaly ? " style=\"color:var(--critical);font-weight:600\"" : "") << ">"
              << s.faults[a][i] << "</td>";
        }
        o << "</tr>";
    }
    o << "</tbody></table></div></details>";
}

void workingSetChart(std::ostream& o, const WorkingSetReport& ws) {
    if (ws.sizes.empty()) return;
    const size_t maxPts = 400;
    size_t bucket = (ws.sizes.size() + maxPts - 1) / maxPts;
    std::vector<int> pts;
    for (size_t i = 0; i < ws.sizes.size(); i += bucket) {
        int m = 0;
        for (size_t j = i; j < std::min(ws.sizes.size(), i + bucket); ++j) m = std::max(m, ws.sizes[j]);
        pts.push_back(m);
    }
    const double W = 760, H = 240, l = 40, r = 90, t = 14, bt = 30;
    const double pw = W - l - r, ph = H - t - bt;
    const double vmax = niceMax(std::max(ws.peak, ws.frames) * 1.1);
    auto X = [&](size_t i) { return l + (pts.size() > 1 ? pw * i / (pts.size() - 1) : 0.0); };
    auto Y = [&](double v) { return t + ph * (1 - v / vmax); };

    o << "<svg viewBox=\"0 0 " << W << ' ' << H << "\" role=\"img\" aria-label=\"Working set size over time\">";
    for (int k = 0; k <= 4; ++k) {
        double v = vmax * k / 4, y = Y(v);
        o << "<line class=\"" << (k == 0 ? "baseline" : "gridline") << "\" x1=\"" << l << "\" y1=\"" << num(y)
          << "\" x2=\"" << W - r << "\" y2=\"" << num(y) << "\"/><text x=\"" << l - 8 << "\" y=\"" << num(y + 4)
          << "\" text-anchor=\"end\">" << fixed(v, v < 10 ? 1 : 0) << "</text>";
    }
    o << "<path class=\"area\" d=\"M" << num(X(0)) << ' ' << num(Y(0));
    for (size_t i = 0; i < pts.size(); ++i) o << " L" << num(X(i)) << ' ' << num(Y(pts[i]));
    o << " L" << num(X(pts.size() - 1)) << ' ' << num(Y(0)) << " Z\"/>";
    o << "<polyline class=\"line\" points=\"";
    for (size_t i = 0; i < pts.size(); ++i) o << num(X(i)) << ',' << num(Y(pts[i])) << ' ';
    o << "\"/>";
    double fy = Y(ws.frames);
    o << "<line class=\"refline\" x1=\"" << l << "\" y1=\"" << num(fy) << "\" x2=\"" << W - r << "\" y2=\""
      << num(fy) << "\"/><text class=\"ink\" x=\"" << W - r + 8 << "\" y=\"" << num(fy + 4) << "\">frames = "
      << ws.frames << "</text>";
    double slot = pts.size() > 1 ? pw / (pts.size() - 1) : pw;
    for (size_t i = 0; i < pts.size(); ++i) {
        size_t from = i * bucket, to = std::min(ws.sizes.size(), from + bucket) - 1;
        std::string tip = from == to ? cat("t=", from, ": |WS| = ", pts[i])
                                     : cat("t=", from, "..", to, ": max |WS| = ", pts[i]);
        o << "<rect class=\"hit-target\" x=\"" << num(X(i) - slot / 2) << "\" y=\"" << t << "\" width=\""
          << num(std::max(slot, 2.0)) << "\" height=\"" << num(ph) << "\" data-tip=\"" << tip << "\"/>";
    }
    o << "<text x=\"" << l << "\" y=\"" << H - 8 << "\">t = 0</text><text x=\"" << W - r << "\" y=\"" << H - 8
      << "\" text-anchor=\"end\">t = " << ws.sizes.size() - 1 << "</text></svg>";
}

void stackHistogram(std::ostream& o, const StackDistanceReport& sd, int frames) {
    size_t maxD = std::min<size_t>(sd.histogram.size() - 1, 30);
    if (maxD == 0) {
        o << "<p class=\"muted\">No page was ever re-referenced.</p>";
        return;
    }
    long long peak = 1;
    for (size_t d = 1; d <= maxD; ++d) peak = std::max(peak, sd.histogram[d]);
    const double W = 760, H = 220, l = 40, r = 16, t = 14, bt = 30;
    const double pw = W - l - r, ph = H - t - bt, vmax = niceMax(static_cast<double>(peak));
    const double slot = pw / maxD, bw = std::min(24.0, slot - 2);
    o << "<div class=\"legend\"><span><i class=\"sw\" style=\"background:var(--series-1)\"></i>within " << frames
      << " frames (LRU hit)</span><span><i class=\"sw\" style=\"background:var(--axis)\"></i>beyond memory (LRU fault)</span></div>";
    o << "<svg viewBox=\"0 0 " << W << ' ' << H << "\" role=\"img\" aria-label=\"LRU stack distance histogram\">";
    for (int k = 0; k <= 4; ++k) {
        double y = t + ph * (1 - k / 4.0);
        o << "<line class=\"" << (k == 0 ? "baseline" : "gridline") << "\" x1=\"" << l << "\" y1=\"" << num(y)
          << "\" x2=\"" << W - r << "\" y2=\"" << num(y) << "\"/><text x=\"" << l - 8 << "\" y=\"" << num(y + 4)
          << "\" text-anchor=\"end\">" << tick(vmax * k / 4) << "</text>";
    }
    for (size_t d = 1; d <= maxD; ++d) {
        double x = l + slot * (d - 1) + (slot - bw) / 2;
        double h = ph * sd.histogram[d] / vmax;
        if (h > 0)
            o << vBar(x, t + ph, bw, h, static_cast<int>(d) <= frames ? "bar" : "bar ref",
                      cat("distance ", d, ": ", sd.histogram[d], " re-references"));
        if (maxD <= 15 || d % 2 == 1)
            o << "<text x=\"" << num(x + bw / 2) << "\" y=\"" << H - 12 << "\" text-anchor=\"middle\">" << d << "</text>";
    }
    o << "</svg><p class=\"muted\">Cold (first-touch) misses: " << sd.coldMisses
      << ". LRU with f frames faults on every re-reference at distance &gt; f, so this one histogram predicts LRU for all memory sizes.</p>";
}

void heatmapSvg(std::ostream& o, const Heatmap& h) {
    if (h.pages.empty()) return;
    const size_t cols = h.windowStart.size(), rows = h.pages.size();
    const double cw = std::max(6.0, std::min(20.0, 700.0 / cols));
    const double ch = std::max(9.0, std::min(18.0, 360.0 / rows));
    const double l = 44, t = 6, W = l + cw * cols + 6, H = t + ch * rows + 24;
    o << "<svg viewBox=\"0 0 " << num(W) << ' ' << num(H) << "\" style=\"max-width:" << num(W * 1.2)
      << "px\" role=\"img\" aria-label=\"Locality heatmap\">";
    for (size_t r = 0; r < rows; ++r) {
        double y = t + ch * (rows - 1 - r);
        if (rows <= 24 || r % 2 == 0)
            o << "<text x=\"" << l - 6 << "\" y=\"" << num(y + ch / 2 + 4) << "\" text-anchor=\"end\">p" << h.pages[r] << "</text>";
        for (size_t c = 0; c < cols; ++c) {
            int v = h.counts[r][c];
            int level = v == 0 ? 0 : std::max(1, static_cast<int>(std::ceil(7.0 * v / std::max(1, h.maxCount))));
            level = std::min(level, 7);
            int to = h.windowStart[c] + h.windowSize - 1;
            o << "<rect class=\"h" << level << "\" x=\"" << num(l + c * cw) << "\" y=\"" << num(y) << "\" width=\""
              << num(cw - 1) << "\" height=\"" << num(ch - 1) << "\" rx=\"1.5\" data-tip=\"page " << h.pages[r]
              << ", t=" << h.windowStart[c] << ".." << to << ": " << v << " reference(s)\"/>";
        }
    }
    o << "<text x=\"" << l << "\" y=\"" << num(H - 6) << "\">t = 0</text><text x=\"" << num(l + cw * cols)
      << "\" y=\"" << num(H - 6) << "\" text-anchor=\"end\">time &#8594;</text></svg>";
}

std::string jsonString(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '<': out += "\\u003c"; break;  // keeps "</script>" out of the page
            default:
                if (c < 0x20) out += cat("\\u00", "0123456789abcdef"[c >> 4], "0123456789abcdef"[c & 15]);
                else out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

// Interactive replay: play / pause / step through every policy's decisions.
void replaySection(std::ostream& o, const ReportBundle& b) {
    if (b.results.empty() || b.results.front().steps.empty()) return;
    const size_t limit = 200;
    const size_t n = std::min(limit, b.results.front().steps.size());

    o << "<section><h2>Animated replay</h2><p class=\"lead\">Watch a policy work reference by reference: green = "
         "hit, red = page fault (the page that was loaded). Click a reference to jump to it.</p>"
         "<div class=\"rp-controls\"><select id=\"rp-algo\" aria-label=\"Policy\"></select>"
         "<button type=\"button\" id=\"rp-reset\">Reset</button><button type=\"button\" id=\"rp-back\">Back</button>"
         "<button type=\"button\" id=\"rp-play\" class=\"primary\">Play</button>"
         "<button type=\"button\" id=\"rp-step\">Step</button>"
         "<select id=\"rp-speed\" aria-label=\"Speed\"><option value=\"0.5\">0.5x</option><option value=\"1\" selected>1x</option>"
         "<option value=\"2\">2x</option><option value=\"4\">4x</option></select>"
         "<input type=\"range\" id=\"rp-slider\" min=\"0\" max=\"" << n << "\" value=\"0\" aria-label=\"Position\">"
         "<span id=\"rp-counts\"></span></div>"
         "<div id=\"rp-refs\"></div><div id=\"rp-frames\"></div><p id=\"rp-status\"></p>";
    if (n < b.results.front().steps.size())
        o << "<p class=\"muted\">Replay shows the first " << n << " of " << b.results.front().steps.size()
          << " references.</p>";

    // Compact JSON: per step [frames, frameUsed, hit, victim, victimDirty, reason].
    o << "<script type=\"application/json\" id=\"replay-data\">{\"refs\":[";
    const auto& first = b.results.front().steps;
    for (size_t i = 0; i < n; ++i)
        o << (i ? "," : "") << "{\"p\":" << first[i].ref.page << ",\"w\":" << (first[i].ref.isWrite() ? 1 : 0) << "}";
    o << "],\"algos\":[";
    for (size_t a = 0; a < b.results.size(); ++a) {
        const SimulationResult& r = b.results[a];
        o << (a ? "," : "") << "{\"key\":" << jsonString(r.key) << ",\"name\":" << jsonString(r.algorithm)
          << ",\"frames\":" << r.frames << ",\"s\":[";
        for (size_t i = 0; i < n; ++i) {
            const StepRecord& s = r.steps[i];
            o << (i ? "," : "") << "[[";
            for (size_t k = 0; k < s.frames.size(); ++k) o << (k ? "," : "") << s.frames[k];
            o << "]," << s.frameUsed << ',' << (s.hit ? 1 : 0) << ',' << s.victimPage << ','
              << (s.victimDirty ? 1 : 0) << ',' << jsonString(s.reason) << ']';
        }
        o << "]}";
    }
    o << "]}</script></section>";
}

void stepTables(std::ostream& o, const ReportBundle& b) {
    const size_t limit = 60;
    for (const auto& r : b.results) {
        if (r.steps.empty()) continue;
        size_t n = std::min(limit, r.steps.size());
        o << "<details><summary>" << r.key << " &#8212; " << htmlEscape(r.algorithm) << " <span>&#183; "
          << r.faults << " faults, " << r.hits << " hits</span></summary><div class=\"scroll\"><table class=\"steps\">";
        o << "<tr><th>Ref</th>";
        for (size_t i = 0; i < n; ++i)
            o << "<th>" << r.steps[i].ref.page << (r.steps[i].ref.isWrite() ? "w" : "") << "</th>";
        o << "</tr>";
        for (int k = 0; k < r.frames; ++k) {
            o << "<tr><td>Frame " << k << "</td>";
            for (size_t i = 0; i < n; ++i) {
                const StepRecord& s = r.steps[i];
                const char* cls = s.frameUsed == k ? (s.hit ? " class=\"hit\"" : " class=\"fault\"") : "";
                o << "<td" << cls << ">" << (s.frames[k] == EMPTY ? "" : std::to_string(s.frames[k])) << "</td>";
            }
            o << "</tr>";
        }
        o << "<tr><td>Evicted</td>";
        for (size_t i = 0; i < n; ++i) {
            const StepRecord& s = r.steps[i];
            o << "<td>" << (s.victimPage == EMPTY ? "" : std::to_string(s.victimPage) + (s.victimDirty ? "*" : "")) << "</td>";
        }
        o << "</tr><tr><td>Result</td>";
        for (size_t i = 0; i < n; ++i)
            o << (r.steps[i].hit ? "<td class=\"res-h\">H</td>" : "<td class=\"res-f\">F</td>");
        o << "</tr></table></div>";
        if (n < r.steps.size()) o << "<p class=\"muted\">Showing the first " << n << " of " << r.steps.size() << " references.</p>";
        o << "<p class=\"muted\">H = hit (green cell), F = fault (red cell = page loaded), * = dirty victim written back.</p>";
        o << "<ol class=\"log\" start=\"0\">";
        for (size_t i = 0; i < n; ++i)
            o << "<li>ref " << r.steps[i].ref.page << ": " << htmlEscape(r.steps[i].reason) << "</li>";
        o << "</ol></details>";
    }
}

}  // namespace

// ---------------------------------------------------------------- bundle --

ReportBundle buildReportBundle(const Trace& trace, const SimulationConfig& config,
                               const std::vector<std::string>& keys, int sweepMaxFrames,
                               int workingSetWindow) {
    ReportBundle b;
    b.trace = trace;
    b.config = config;
    b.config.recordSteps = true;
    b.config.explain = true;
    b.results = Simulator::runAll(keys, trace, b.config);
    b.optFaults = optimalFaults(trace, config);

    std::vector<std::string> sweepKeys = keys;
    if (std::find(sweepKeys.begin(), sweepKeys.end(), "OPT") == sweepKeys.end()) sweepKeys.push_back("OPT");
    int maxF = std::max(2, sweepMaxFrames > 0 ? sweepMaxFrames : std::max(config.frames + 3, std::min(trace.distinctPages(), 12)));
    b.sweep = frameSweep(trace, sweepKeys, 1, maxF, config);
    b.anomalies = detectBeladyAnomalies(b.sweep);
    b.workingSet = analyzeWorkingSet(trace, std::max(1, workingSetWindow), config.frames);
    b.stackDistance = analyzeStackDistances(trace, maxF);
    b.profile = profileTrace(trace, config.frames);
    b.heatmap = buildHeatmap(trace, 60, 32);
    for (const auto& k : keys) b.inclusion.push_back(checkInclusion(k, trace, config.frames, config));
    return b;
}

// ------------------------------------------------------------------ HTML --

void writeHtmlReport(const ReportBundle& b, const std::string& path) {
    std::ofstream o = openOut(path);
    auto ranked = rankResults(b.results, b.optFaults);
    const SimulationResult* best = nullptr;
    for (const auto& rr : ranked) if (rr.result->key != "OPT") { best = rr.result; break; }
    int bestTies = 0;
    for (const auto& r : b.results)
        if (best && &r != best && r.key != "OPT" && r.faults == best->faults && r.writeBacks == best->writeBacks)
            ++bestTies;

    o << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
         "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
         "<title>Page Replacement Report</title><style>" << CSS << "</style></head><body><main>";

    o << "<header><div><h1>Page Replacement Analysis</h1><p class=\"sub\">Trace <b>" << htmlEscape(b.trace.name())
      << "</b> &#183; " << b.config.frames << " frames &#183; " << b.results.size()
      << " policies</p><p class=\"muted\">Generated " << timestamp()
      << " by Page Replacement Algorithm Simulator (C++)</p></div>"
         "<button class=\"theme\" id=\"theme\" type=\"button\">Toggle theme</button></header>";

    // Stat tiles.
    o << "<div class=\"tiles\">";
    o << "<div class=\"tile\"><div class=\"label\">References</div><div class=\"value\">" << b.trace.size()
      << "</div><div class=\"note\">" << b.trace.distinctPages() << " distinct pages</div></div>";
    o << "<div class=\"tile\"><div class=\"label\">Physical frames</div><div class=\"value\">" << b.config.frames
      << "</div><div class=\"note\">" << b.profile.localityClass << "</div></div>";
    if (best)
        o << "<div class=\"tile\"><div class=\"label\">Best practical policy</div><div class=\"value\">" << best->key
          << "</div><div class=\"note\">" << best->faults << " faults &#183; hit ratio " << percent(best->hitRatio(), 1)
          << (bestTies > 0 ? cat(" &#183; tied with ", bestTies, " more") : std::string()) << "</div></div>";
    o << "<div class=\"tile\"><div class=\"label\">Optimal benchmark</div><div class=\"value\">" << b.optFaults
      << "</div><div class=\"note\">minimum possible faults</div></div>";
    o << "<div class=\"tile\"><div class=\"label\">Belady's anomaly</div><div class=\"value\">"
      << (b.anomalies.empty() ? "None" : std::to_string(b.anomalies.size()))
      << "</div><div class=\"note\">frames 1&#8211;" << b.sweep.frameCounts.back() << " scanned</div></div>";
    o << "</div>";

    o << "<section><h2>Reference string</h2><p class=\"lead\">The same trace is replayed independently for every policy."
         " A trailing w marks a write (sets the dirty bit).</p><p style=\"font-family:ui-monospace,Consolas,monospace;"
         "font-size:13px;word-break:break-word;margin:0\">" << htmlEscape(b.trace.toString(400)) << "</p></section>";

    replaySection(o, b);

    o << "<section><h2>Comparison</h2><p class=\"lead\">Ranked by page faults (ties broken by dirty write-backs). "
         "EAT uses memory access " << formatNs(b.config.memoryAccessNs) << ", fault service "
      << formatNs(b.config.faultServiceNs) << " and write-back " << formatNs(b.config.writeBackNs) << ".</p>";
    comparisonTable(o, b);
    o << "</section>";

    o << "<section><h2>Page faults by policy</h2><p class=\"lead\">With " << b.config.frames << " frames on "
      << b.trace.size() << " references.</p>";
    faultBarChart(o, b);
    o << "</section>";

    o << "<section><h2>Faults vs number of frames</h2><p class=\"lead\">Each panel replays the trace with 1 to "
      << b.sweep.frameCounts.back() << " frames, on a shared scale. A curve that rises when memory grows is Belady's anomaly.</p>";
    sweepMultiples(o, b);
    if (b.anomalies.empty()) {
        o << "<p><span class=\"pill good\">No anomaly</span> No policy faulted more when a frame was added.</p>";
    } else {
        o << "<ul class=\"insights\">";
        for (const auto& a : b.anomalies)
            o << "<li><span class=\"pill bad\">Belady</span> <b>" << a.algorithm << "</b>: " << a.framesFrom
              << " frames &#8594; " << a.faultsFrom << " faults, but " << a.framesTo << " frames &#8594; "
              << a.faultsTo << " faults.</li>";
        o << "</ul>";
    }
    o << "</section>";

    o << "<section><h2>Stack-algorithm check</h2><p class=\"lead\">The inclusion property M(n) &#8838; M(n+1) "
         "must hold at every step for a policy to be immune to Belady's anomaly.</p><div class=\"scroll\"><table>"
         "<thead><tr><th>Policy</th><th class=\"l\">M(" << b.config.frames << ") &#8838; M(" << b.config.frames + 1
      << ")</th><th class=\"l\">Detail</th></tr></thead><tbody>";
    for (const auto& inc : b.inclusion) {
        o << "<tr><td>" << inc.algorithm << "</td><td class=\"l\">"
          << (inc.holds ? "<span class=\"pill good\">holds</span>" : "<span class=\"pill bad\">violated</span>")
          << "</td><td class=\"l\">";
        if (!inc.holds) {
            std::vector<std::string> s1, s2;
            for (int p : inc.smallerSet) s1.push_back(std::to_string(p));
            for (int p : inc.largerSet) s2.push_back(std::to_string(p));
            o << "t=" << inc.violationStep << ": {" << join(s1, ",") << "} &#8840; {" << join(s2, ",") << "}";
        } else {
            o << "<span class=\"muted\">every step</span>";
        }
        o << "</td></tr>";
    }
    o << "</tbody></table></div></section>";

    o << "<section><h2>Working set &amp; thrashing</h2><p class=\"lead\">|WS(t, &#916;)| = distinct pages in the last "
      << b.workingSet.window << " references (Denning). When it stays above the frame count, the process thrashes.</p>";
    o << "<div class=\"kv\"><div><span>Average |WS|:</span> " << fixed(b.workingSet.average, 2)
      << "</div><div><span>Peak:</span> " << b.workingSet.peak << "</div><div><span>90th percentile:</span> "
      << b.workingSet.percentile90 << "</div><div><span>Above memory:</span> "
      << percent(b.workingSet.fractionAboveFrames, 1) << " of the time</div><div><span>Thrashing risk:</span> "
      << (b.workingSet.thrashingRisk ? "<span class=\"pill bad\">high</span>" : "<span class=\"pill good\">low</span>")
      << "</div><div><span>Suggested frames:</span> &#8805; " << b.workingSet.percentile90 << "</div></div>";
    workingSetChart(o, b.workingSet);
    o << "</section>";

    o << "<section><h2>Trace profile</h2><div class=\"kv\"><div><span>Locality class:</span> "
      << htmlEscape(b.profile.localityClass) << "</div><div><span>Reuse ratio:</span> "
      << percent(b.profile.reuseRatio, 1) << "</div><div><span>Sequential refs:</span> "
      << percent(b.profile.sequentialFraction, 1) << "</div><div><span>Writes:</span> "
      << percent(b.profile.writeFraction, 1) << "</div><div><span>Hot 20% share:</span> "
      << percent(b.profile.hotSetShare, 1) << "</div><div><span>Median stack distance:</span> "
      << fixed(b.profile.medianStackDistance, 1) << "</div></div><ul class=\"insights\">";
    for (const auto& s : b.profile.insights) o << "<li>" << htmlEscape(s) << "</li>";
    o << "</ul></section>";

    o << "<section><h2>LRU stack distances</h2><p class=\"lead\">How deep in the LRU stack each re-referenced page was found (Mattson et al., 1970).</p>";
    stackHistogram(o, b.stackDistance, b.config.frames);
    o << "</section>";

    o << "<section><h2>Locality heatmap</h2><p class=\"lead\">Each row is a page, each column a window of "
      << b.heatmap.windowSize << " reference(s); a more intense colour means more references. Horizontal bands = temporal "
         "locality, diagonals = sequential scans.</p>";
    heatmapSvg(o, b.heatmap);
    o << "</section>";

    o << "<section><h2>Step-by-step frame tables</h2><p class=\"lead\">Expand a policy to see the frame contents "
         "after every reference and the reason behind each eviction.</p>";
    stepTables(o, b);
    o << "</section>";

    o << "<footer>Page Replacement Algorithm Simulator &#183; Operating Systems PBL &#183; Jaypee Institute of Information Technology</footer>";
    o << "</main><div id=\"tip\" hidden></div><script>" << SCRIPT << "</script></body></html>\n";
}

// -------------------------------------------------------------- Markdown --

void writeMarkdownReport(const ReportBundle& b, const std::string& path) {
    std::ofstream o = openOut(path);
    auto ranked = rankResults(b.results, b.optFaults);
    o << "# Page Replacement Analysis\n\n";
    o << "- **Trace:** " << b.trace.name() << " (" << b.trace.size() << " references, "
      << b.trace.distinctPages() << " distinct pages)\n";
    o << "- **Frames:** " << b.config.frames << "\n- **Generated:** " << timestamp() << "\n\n";
    o << "```\n" << b.trace.toString(200) << "\n```\n\n## Comparison\n\n";
    o << "| # | Policy | Faults | Hits | Hit ratio | Fault ratio | Write-backs | EAT | vs OPT |\n";
    o << "|---|---|---:|---:|---:|---:|---:|---:|---:|\n";
    for (const auto& rr : ranked) {
        const auto& r = *rr.result;
        o << "| " << rr.rank << " | " << r.key << " | " << r.faults << " | " << r.hits << " | "
          << percent(r.hitRatio()) << " | " << percent(r.faultRatio()) << " | " << r.writeBacks << " | "
          << formatNs(r.effectiveAccessNs) << " | "
          << (rr.extraFaultsVsOpt ? "+" + std::to_string(rr.extraFaultsVsOpt) : std::string("=")) << " |\n";
    }
    o << "\n## Faults vs frames\n\n| Policy |";
    for (int f : b.sweep.frameCounts) o << ' ' << f << " |";
    o << "\n|---|";
    for (size_t i = 0; i < b.sweep.frameCounts.size(); ++i) o << "---:|";
    o << "\n";
    for (size_t a = 0; a < b.sweep.algorithms.size(); ++a) {
        o << "| " << b.sweep.algorithms[a] << " |";
        for (long long v : b.sweep.faults[a]) o << ' ' << v << " |";
        o << "\n";
    }
    o << "\n## Belady's anomaly\n\n";
    if (b.anomalies.empty()) o << "No anomaly detected.\n";
    for (const auto& a : b.anomalies)
        o << "- **" << a.algorithm << "**: " << a.framesFrom << " frames -> " << a.faultsFrom << " faults, "
          << a.framesTo << " frames -> " << a.faultsTo << " faults\n";
    o << "\n## Working set\n\n- Window: " << b.workingSet.window << "\n- Average |WS|: "
      << fixed(b.workingSet.average, 2) << ", peak " << b.workingSet.peak << "\n- Thrashing risk: "
      << (b.workingSet.thrashingRisk ? "high" : "low") << "\n\n## Trace insights\n\n";
    o << "Locality class: **" << b.profile.localityClass << "**\n\n";
    for (const auto& s : b.profile.insights) o << "- " << s << "\n";
}

// ------------------------------------------------------------------- CSV --

void writeSummaryCsv(const std::vector<SimulationResult>& results, long long optFaults, const std::string& path) {
    std::ofstream o = openOut(path);
    o << "algorithm,name,frames,references,hits,faults,hit_ratio,fault_ratio,compulsory,capacity,"
         "evictions,write_backs,eat_ns,extra_vs_opt,runtime_us\n";
    for (const auto& r : results) {
        o << r.key << ',' << csvField(r.algorithm) << ',' << r.frames << ',' << r.references << ',' << r.hits << ','
          << r.faults << ',' << fixed(r.hitRatio(), 4) << ',' << fixed(r.faultRatio(), 4) << ','
          << r.compulsoryFaults << ',' << r.capacityFaults << ',' << r.evictions << ',' << r.writeBacks << ','
          << fixed(r.effectiveAccessNs, 2) << ',' << (r.faults - optFaults) << ',' << fixed(r.runtimeMicros, 2) << '\n';
    }
}

void writeStepsCsv(const std::vector<SimulationResult>& results, const std::string& path) {
    std::ofstream o = openOut(path);
    o << "algorithm,t,page,op,result,fault_kind,frame,victim,victim_dirty,frames,reason\n";
    std::string line;
    for (const auto& r : results) {
        for (const auto& s : r.steps) {
            line.assign(r.key);
            line += ',';
            appendInt(line, s.index);
            line += ',';
            appendInt(line, s.ref.page);
            line += s.ref.isWrite() ? ",W," : ",R,";
            line += s.hit ? "HIT," : "FAULT,";
            line += s.faultKind == FaultKind::Compulsory ? "compulsory" : s.faultKind == FaultKind::Capacity ? "capacity" : "";
            line += ',';
            appendInt(line, s.frameUsed);
            line += ',';
            if (s.victimPage != EMPTY) appendInt(line, s.victimPage);
            line += s.victimDirty ? ",1," : ",0,";
            for (size_t k = 0; k < s.frames.size(); ++k) {
                if (k) line += ' ';
                if (s.frames[k] == EMPTY) line += '-';
                else appendInt(line, s.frames[k]);
            }
            line += ',';
            line += csvField(s.reason);
            line += '\n';
            o << line;
        }
    }
}

void writeSweepCsv(const SweepResult& sweep, const std::string& path) {
    std::ofstream o = openOut(path);
    o << "frames";
    for (const auto& a : sweep.algorithms) o << ',' << a;
    o << '\n';
    for (size_t i = 0; i < sweep.frameCounts.size(); ++i) {
        o << sweep.frameCounts[i];
        for (size_t a = 0; a < sweep.algorithms.size(); ++a) o << ',' << sweep.faults[a][i];
        o << '\n';
    }
}

}  // namespace prs
