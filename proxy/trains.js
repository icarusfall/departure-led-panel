// Converts a Darwin (LDBWS) departure board into the compact shape the LED
// board polls, and filters it by direction.

// A train counts as "north" (towards central London) if it calls at one of
// these stations soon after leaving. Southbound is everything else.
export const CENTRAL_LONDON = new Set([
  'LBG', // London Bridge
  'BFR', // London Blackfriars
  'EPH', // Elephant & Castle
  'VIC', // London Victoria
  'CTK', // City Thameslink
  'ZFD', // Farringdon
  'STP', // St Pancras International
  'CHX', // Charing Cross
  'CST', // Cannon Street
  'WAE', // Waterloo East
]);

// How many upcoming stops to look at when deciding direction. Keeps loop
// services (out to Sutton/Wimbledon and back) counted as southbound.
const LOOKAHEAD = 8;

const asArray = (v) => (Array.isArray(v) ? v : v ? [v] : []);

// Darwin nests calling points as [{ callingPoint: [ ... ] }, ...] (one list
// per portion of a splitting train). Use the first portion.
function callingPoints(service) {
  const lists = asArray(service.subsequentCallingPoints);
  return lists.length ? asArray(lists[0].callingPoint) : [];
}

export function isNorthbound(service) {
  return callingPoints(service)
    .slice(0, LOOKAHEAD)
    .some((cp) => CENTRAL_LONDON.has(cp.crs));
}

// Minutes from `nowMinutes` (London time, minutes since midnight) until a
// "HH:MM" time, allowing for services just after midnight.
export function minutesUntil(hhmm, nowMinutes) {
  const m = /^(\d{2}):(\d{2})$/.exec(hhmm || '');
  if (!m) return null;
  let diff = Number(m[1]) * 60 + Number(m[2]) - nowMinutes;
  if (diff < -720) diff += 1440;
  if (diff > 720) diff -= 1440;
  return diff;
}

export function londonMinutesNow(date = new Date()) {
  const parts = new Intl.DateTimeFormat('en-GB', {
    timeZone: 'Europe/London',
    hour: '2-digit',
    minute: '2-digit',
    hourCycle: 'h23',
  }).formatToParts(date);
  const get = (t) => Number(parts.find((p) => p.type === t).value);
  return get('hour') * 60 + get('minute');
}

// Short status text that fits the right-hand column of the board.
function statusFor(service) {
  if (service.isCancelled || service.etd === 'Cancelled') return 'Canc';
  if (service.etd === 'Delayed') return 'Delay';
  return null; // board shows minutes instead
}

export function shapeBoard(board, { dir = 'all', nowMinutes, max = 8 } = {}) {
  const services = asArray(board.trainServices)
    .filter((s) => dir === 'all' || (dir === 'north') === isNorthbound(s))
    .map((s) => {
      const expected = /^\d{2}:\d{2}$/.test(s.etd || '') ? s.etd : s.std;
      const mins = minutesUntil(expected, nowMinutes);
      return {
        std: s.std,
        etd: s.etd,
        dest: asArray(s.destination).map((d) => d.locationName).join(' & '),
        plat: s.platform || null,
        mins: mins === null ? null : Math.max(0, mins),
        status: statusFor(s),
      };
    })
    .slice(0, max);

  return {
    station: board.locationName,
    crs: board.crs,
    dir,
    generatedAt: board.generatedAt,
    services,
  };
}
