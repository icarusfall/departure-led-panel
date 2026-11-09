import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { shapeBoard, minutesUntil } from '../trains.js';

const board = JSON.parse(
  readFileSync(new URL('./sample-board.json', import.meta.url), 'utf8'),
);
const now = 22 * 60 + 30; // 22:30

test('north keeps only trains calling at central London', () => {
  const out = shapeBoard(board, { dir: 'north', nowMinutes: now });
  assert.deepEqual(out.services.map((s) => s.dest),
    ['London Victoria', 'London Blackfriars']);
});

test('south keeps the rest', () => {
  const out = shapeBoard(board, { dir: 'south', nowMinutes: now });
  assert.deepEqual(out.services.map((s) => s.dest), ['East Croydon', 'Epsom']);
});

test('minutes use the expected time when late', () => {
  const out = shapeBoard(board, { dir: 'south', nowMinutes: now });
  assert.equal(out.services[0].mins, 11); // etd 22:41
  assert.equal(out.services[0].status, null);
});

test('cancelled trains are flagged', () => {
  const out = shapeBoard(board, { dir: 'north', nowMinutes: now });
  assert.equal(out.services[1].status, 'Canc');
});

test('minutes wrap past midnight', () => {
  assert.equal(minutesUntil('00:05', 23 * 60 + 55), 10);
  assert.equal(minutesUntil('23:55', 5), -10);
});
