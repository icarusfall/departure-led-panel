// Train departures proxy for the home LED departure board.
//
//   GET /trains/:crs?dir=north|south|all   (header X-Board-Token required)
//   GET /health
//
// Environment variables (set in Railway, never committed):
//   RDM_KEY      Rail Data Marketplace consumer key for Live Departure Board
//   PROXY_TOKEN  shared secret the board sends in X-Board-Token
//   LDB_URL      optional override of the LDBWS base URL
//   FIXTURE      optional path to a saved board JSON, for local testing

import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { shapeBoard, londonMinutesNow } from './trains.js';

const PORT = Number(process.env.PORT) || 3000;
const { RDM_KEY, PROXY_TOKEN, FIXTURE } = process.env;
const LDB_URL =
  process.env.LDB_URL ||
  'https://api1.raildata.org.uk/1010-live-departure-board-dep1_2/LDBWS/api/20220120';
const CACHE_MS = 30_000;
const ALLOWED_CRS = new Set(['STE', 'SRC']); // Streatham, Streatham Common

const cache = new Map(); // crs -> { at, board }

async function fetchBoard(crs) {
  const hit = cache.get(crs);
  if (hit && Date.now() - hit.at < CACHE_MS) return hit.board;

  let board;
  if (FIXTURE) {
    board = JSON.parse(await readFile(FIXTURE, 'utf8'));
  } else {
    const url = `${LDB_URL}/GetDepBoardWithDetails/${crs}?numRows=20&timeWindow=120`;
    const res = await fetch(url, {
      headers: { 'x-apikey': RDM_KEY },
      signal: AbortSignal.timeout(10_000),
    });
    if (!res.ok) throw new Error(`Darwin HTTP ${res.status}`);
    board = await res.json();
  }
  cache.set(crs, { at: Date.now(), board });
  return board;
}

// An explicit Content-Length avoids chunked responses, which the board's
// streaming JSON parser can't read.
function send(res, status, body) {
  const json = Buffer.from(JSON.stringify(body));
  res.writeHead(status, {
    'content-type': 'application/json',
    'content-length': json.length,
  });
  res.end(json);
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://localhost');
  if (url.pathname === '/health') return send(res, 200, { ok: true });

  const m = /^\/trains\/([A-Za-z]{3})$/.exec(url.pathname);
  if (!m || req.method !== 'GET') return send(res, 404, { error: 'Not found' });
  if (!PROXY_TOKEN || req.headers['x-board-token'] !== PROXY_TOKEN)
    return send(res, 401, { error: 'Bad token' });

  const crs = m[1].toUpperCase();
  if (!ALLOWED_CRS.has(crs)) return send(res, 400, { error: 'Unknown station' });
  const dir = ['north', 'south'].includes(url.searchParams.get('dir'))
    ? url.searchParams.get('dir')
    : 'all';

  try {
    const board = await fetchBoard(crs);
    send(res, 200, shapeBoard(board, { dir, nowMinutes: londonMinutesNow() }));
  } catch (err) {
    console.error(`${crs}: ${err.message}`);
    send(res, 502, { error: err.message });
  }
});

server.listen(PORT, () => {
  console.log(`Train proxy on :${PORT}${FIXTURE ? ` (fixture ${FIXTURE})` : ''}`);
  if (!RDM_KEY && !FIXTURE) console.warn('RDM_KEY is not set');
  if (!PROXY_TOKEN) console.warn('PROXY_TOKEN is not set; all requests will be refused');
});
