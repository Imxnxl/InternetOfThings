/*
 * informe_a_docx.js - Convierte docs/INFORME_TECNICO.md en un .docx entregable.
 *
 * Traduce el subconjunto de Markdown que usa el informe: encabezados, parrafos,
 * listas, citas, bloques de codigo, tablas, imagenes y enfasis en linea.
 *
 * Uso:  node scripts/informe_a_docx.js
 *
 * Requiere el paquete npm "docx". Si no esta instalado en este directorio:
 *     npm install docx
 */
const fs = require("fs");
const path = require("path");
const {
  Document, Packer, Paragraph, TextRun, HeadingLevel, AlignmentType,
  Table, TableRow, TableCell, WidthType, ShadingType, BorderStyle,
  ImageRun, PageBreak, Footer, PageNumber, TabStopType,
} = require("docx");

const RAIZ = path.dirname(__dirname);
const ENTRADA = path.join(RAIZ, "docs", "INFORME_TECNICO.md");
const SALIDA = path.join(RAIZ, "docs", "Informe_tecnico_FreeRTOS_ESP32-C6.docx");

// Autor que queda en los metadatos del documento. Se puede sobrescribir con
// la variable de entorno AUTOR.
const AUTOR = process.env.AUTOR || "Imxnxl";

// Ancho util de la pagina A4 con margenes de 1 pulgada, en DXA (1440 = 1").
const ANCHO_UTIL = 11906 - 1440 * 2;

const TINTA = "1A1A1A";
const TINTA_2 = "52514E";
const ACENTO = "1C5CAB";
const FONDO_COD = "F2F2EF";
const FONDO_CAB = "E8ECF3";

/* ------------------------------------------------------------ inline ---- */

/**
 * Convierte el enfasis en linea de Markdown (**negrita**, *cursiva*, `codigo`)
 * en TextRun con el formato correspondiente.
 */
function runs(texto, base = {}) {
  const salida = [];
  // Se procesa con una sola pasada para no romper los delimitadores anidados.
  const re = /(\*\*[^*]+\*\*|\*[^*]+\*|`[^`]+`|\[[^\]]+\]\([^)]+\))/g;
  let ultimo = 0;
  let m;

  const plano = (t) => {
    if (t) salida.push(new TextRun({ text: t, color: TINTA, ...base }));
  };

  while ((m = re.exec(texto)) !== null) {
    plano(texto.slice(ultimo, m.index));
    const t = m[0];
    if (t.startsWith("**")) {
      salida.push(new TextRun({ text: t.slice(2, -2), bold: true, color: TINTA, ...base }));
    } else if (t.startsWith("`")) {
      salida.push(new TextRun({
        text: t.slice(1, -1), font: "Consolas", size: 19,
        color: "9A3412", shading: { type: ShadingType.CLEAR, fill: FONDO_COD },
        ...base,
      }));
    } else if (t.startsWith("[")) {
      const etiqueta = t.slice(1, t.indexOf("]"));
      salida.push(new TextRun({ text: etiqueta, color: ACENTO, ...base }));
    } else {
      salida.push(new TextRun({ text: t.slice(1, -1), italics: true, color: TINTA, ...base }));
    }
    ultimo = m.index + t.length;
  }
  plano(texto.slice(ultimo));
  return salida.length ? salida : [new TextRun({ text: "", ...base })];
}

/* ------------------------------------------------------------- bloques -- */

function parrafo(texto, extra = {}) {
  return new Paragraph({ children: runs(texto), spacing: { after: 140 }, ...extra });
}

function bloqueCodigo(lineas) {
  // Una tabla de una celda da el fondo continuo que un parrafo sombreado no
  // consigue mantener entre lineas.
  return new Table({
    width: { size: ANCHO_UTIL, type: WidthType.DXA },
    columnWidths: [ANCHO_UTIL],
    borders: bordes("DDDCD4"),
    rows: [new TableRow({
      children: [new TableCell({
        width: { size: ANCHO_UTIL, type: WidthType.DXA },
        shading: { type: ShadingType.CLEAR, fill: FONDO_COD },
        margins: { top: 120, bottom: 120, left: 160, right: 160 },
        children: lineas.map((l) => new Paragraph({
          children: [new TextRun({ text: l || " ", font: "Consolas", size: 17, color: "24292F" })],
          spacing: { after: 0, line: 250 },
        })),
      })],
    })],
  });
}

function bordes(color) {
  const b = { style: BorderStyle.SINGLE, size: 2, color };
  return { top: b, bottom: b, left: b, right: b, insideHorizontal: b, insideVertical: b };
}

function celdasDe(linea) {
  return linea.replace(/^\||\|$/g, "").split("|").map((c) => c.trim());
}

function tabla(lineas) {
  const cabecera = celdasDe(lineas[0]);
  const cuerpo = lineas.slice(2).map(celdasDe);
  const n = cabecera.length;
  const ancho = Math.floor(ANCHO_UTIL / n);
  const anchos = Array(n).fill(ancho);
  anchos[n - 1] = ANCHO_UTIL - ancho * (n - 1);   // deben sumar el total exacto

  const fila = (celdas, esCabecera) => new TableRow({
    tableHeader: esCabecera,
    children: celdas.map((c, i) => new TableCell({
      width: { size: anchos[i], type: WidthType.DXA },
      shading: esCabecera
        ? { type: ShadingType.CLEAR, fill: FONDO_CAB }
        : undefined,
      margins: { top: 80, bottom: 80, left: 110, right: 110 },
      children: [new Paragraph({
        children: runs(c, esCabecera ? { bold: true, size: 19 } : { size: 19 }),
        spacing: { after: 0 },
      })],
    })),
  });

  return new Table({
    width: { size: ANCHO_UTIL, type: WidthType.DXA },
    columnWidths: anchos,
    borders: bordes("C9C8C0"),
    rows: [fila(cabecera, true), ...cuerpo.map((f) => fila(f, false))],
  });
}

function imagen(rutaRelativa, pie) {
  const ruta = path.join(RAIZ, "docs", rutaRelativa);
  if (!fs.existsSync(ruta)) {
    console.warn("  aviso: no se encuentra la imagen", rutaRelativa);
    return [];
  }
  // Se escala al ancho util manteniendo la proporcion original del PNG.
  const buf = fs.readFileSync(ruta);
  const wPx = buf.readUInt32BE(16);
  const hPx = buf.readUInt32BE(20);
  const anchoPt = 468;                       // 6.5 pulgadas en puntos
  const altoPt = Math.round((anchoPt * hPx) / wPx);

  return [
    new Paragraph({
      children: [new ImageRun({
        data: buf, type: "png",
        transformation: { width: anchoPt, height: altoPt },
      })],
      alignment: AlignmentType.CENTER,
      spacing: { before: 160, after: 60 },
    }),
    new Paragraph({
      children: [new TextRun({ text: pie, size: 17, italics: true, color: TINTA_2 })],
      alignment: AlignmentType.CENTER,
      spacing: { after: 220 },
    }),
  ];
}

/* -------------------------------------------------------------- parser -- */

function convertir(md) {
  const lineas = md.split(/\r?\n/);
  const salida = [];
  let i = 0;

  while (i < lineas.length) {
    const l = lineas[i];

    // Bloque de codigo
    if (l.startsWith("```")) {
      const cuerpo = [];
      i++;
      while (i < lineas.length && !lineas[i].startsWith("```")) cuerpo.push(lineas[i++]);
      i++;
      salida.push(bloqueCodigo(cuerpo));
      salida.push(new Paragraph({ text: "", spacing: { after: 140 } }));
      continue;
    }

    // Tabla
    if (l.startsWith("|") && i + 1 < lineas.length && /^\|[\s:|-]+\|$/.test(lineas[i + 1])) {
      const bloque = [];
      while (i < lineas.length && lineas[i].startsWith("|")) bloque.push(lineas[i++]);
      salida.push(tabla(bloque));
      salida.push(new Paragraph({ text: "", spacing: { after: 180 } }));
      continue;
    }

    // Imagen
    const img = l.match(/^!\[([^\]]*)\]\(([^)]+)\)/);
    if (img) {
      salida.push(...imagen(img[2], img[1]));
      i++;
      continue;
    }

    // Encabezados
    const h = l.match(/^(#{1,4})\s+(.*)$/);
    if (h) {
      const nivel = h[1].length;
      const niveles = [HeadingLevel.TITLE, HeadingLevel.HEADING_1,
                       HeadingLevel.HEADING_2, HeadingLevel.HEADING_3];
      salida.push(new Paragraph({
        children: runs(h[2]),
        heading: niveles[nivel - 1],
        spacing: { before: nivel <= 2 ? 320 : 240, after: 140 },
        pageBreakBefore: nivel === 2 && /^\d+\./.test(h[2]),
      }));
      i++;
      continue;
    }

    // Regla horizontal: se ignora, la separacion la dan los encabezados
    if (/^---+$/.test(l.trim())) { i++; continue; }

    // Cita
    if (l.startsWith("> ")) {
      salida.push(new Paragraph({
        children: runs(l.slice(2), { italics: true, color: TINTA_2 }),
        indent: { left: 420 },
        border: { left: { style: BorderStyle.SINGLE, size: 12, color: "C9C8C0", space: 12 } },
        spacing: { before: 120, after: 160 },
      }));
      i++;
      continue;
    }

    // Lista con vinetas
    if (/^[-*]\s+/.test(l)) {
      salida.push(new Paragraph({
        children: runs(l.replace(/^[-*]\s+/, "")),
        bullet: { level: 0 },
        spacing: { after: 90 },
      }));
      i++;
      continue;
    }

    // Lista numerada
    if (/^\d+\.\s+/.test(l)) {
      salida.push(new Paragraph({
        children: runs(l.replace(/^\d+\.\s+/, "")),
        numbering: { reference: "lista-numerada", level: 0 },
        spacing: { after: 90 },
      }));
      i++;
      continue;
    }

    if (l.trim() === "") { i++; continue; }

    salida.push(parrafo(l));
    i++;
  }

  return salida;
}

/* ---------------------------------------------------------------- main -- */

const md = fs.readFileSync(ENTRADA, "utf8");

const doc = new Document({
  creator: AUTOR,
  lastModifiedBy: AUTOR,
  title: "Informe tecnico - Planificacion con FreeRTOS en un solo nucleo (ESP32-C6)",
  description: "Practica sobre planificacion de tareas con FreeRTOS en un microcontrolador de un solo nucleo (ESP32-C6).",
  numbering: {
    config: [{
      reference: "lista-numerada",
      levels: [{
        level: 0, format: "decimal", text: "%1.", alignment: AlignmentType.START,
        style: { paragraph: { indent: { left: 420, hanging: 260 } } },
      }],
    }],
  },
  styles: {
    default: {
      document: { run: { font: "Calibri", size: 21, color: TINTA } },
      title: { run: { font: "Calibri Light", size: 40, bold: true, color: TINTA } },
      heading1: { run: { font: "Calibri Light", size: 30, bold: true, color: TINTA } },
      heading2: { run: { font: "Calibri Light", size: 25, bold: true, color: TINTA } },
      heading3: { run: { font: "Calibri", size: 22, bold: true, color: TINTA_2 } },
    },
  },
  sections: [{
    properties: {
      page: {
        size: { width: 11906, height: 16838 },      // A4
        margin: { top: 1440, bottom: 1440, left: 1440, right: 1440 },
      },
    },
    footers: {
      default: new Footer({
        children: [new Paragraph({
          alignment: AlignmentType.CENTER,
          children: [new TextRun({
            children: ["Pagina ", PageNumber.CURRENT, " de ", PageNumber.TOTAL_PAGES],
            size: 17, color: TINTA_2,
          })],
        })],
      }),
    },
    children: convertir(md),
  }],
});

Packer.toBuffer(doc).then((buf) => {
  fs.writeFileSync(SALIDA, buf);
  console.log("informe generado:", path.relative(RAIZ, SALIDA),
              "(" + (buf.length / 1024).toFixed(0) + " KB)");
});
