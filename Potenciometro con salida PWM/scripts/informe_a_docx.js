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
// La ruta de salida se puede cambiar con la variable de entorno SALIDA (util si
// el .docx esta abierto en Word y no se puede sobrescribir).
const SALIDA = process.env.SALIDA ||
  path.join(RAIZ, "docs", "Informe_tecnico_Potenciometro_PWM_ESP32-C6.docx");

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

/**
 * Ancho y alto en pixeles de un PNG o un JPEG, leidos de su cabecera.
 * En el JPEG se recorren los segmentos hasta el marcador SOFn.
 */
function dimensiones(buf) {
  if (buf.readUInt32BE(0) === 0x89504e47) {
    return { tipo: "png", w: buf.readUInt32BE(16), h: buf.readUInt32BE(20) };
  }
  if (buf.readUInt16BE(0) === 0xffd8) {
    let i = 2;
    while (i < buf.length) {
      const marcador = buf.readUInt16BE(i);
      const largo = buf.readUInt16BE(i + 2);
      const esSOF = marcador >= 0xffc0 && marcador <= 0xffcf &&
                    ![0xffc4, 0xffc8, 0xffcc].includes(marcador);
      if (esSOF) return { tipo: "jpg", h: buf.readUInt16BE(i + 5), w: buf.readUInt16BE(i + 7) };
      i += 2 + largo;
    }
  }
  throw new Error("formato de imagen no soportado (solo PNG o JPEG)");
}

// Recuadro visible en lugar de una evidencia que todavia no existe, para que
// el .docx nunca se genere con un hueco silencioso.
function pendiente(rutaRelativa, pie) {
  return [new Table({
    width: { size: ANCHO_UTIL, type: WidthType.DXA },
    columnWidths: [ANCHO_UTIL],
    borders: bordes("E0A100"),
    rows: [new TableRow({
      children: [new TableCell({
        width: { size: ANCHO_UTIL, type: WidthType.DXA },
        shading: { type: ShadingType.CLEAR, fill: "FFF8E1" },
        margins: { top: 260, bottom: 260, left: 200, right: 200 },
        children: [
          new Paragraph({
            alignment: AlignmentType.CENTER,
            children: [new TextRun({ text: "EVIDENCIA PENDIENTE: " + pie, bold: true, color: "8A6100" })],
          }),
          new Paragraph({
            alignment: AlignmentType.CENTER,
            children: [new TextRun({ text: "docs/" + rutaRelativa, size: 17, color: "8A6100" })],
          }),
        ],
      })],
    })],
  }), new Paragraph({ text: "", spacing: { after: 180 } })];
}

function imagen(rutaRelativa, pie) {
  const ruta = path.join(RAIZ, "docs", rutaRelativa);
  if (!fs.existsSync(ruta)) {
    console.warn("  aviso: falta la imagen", rutaRelativa, "(se deja un recuadro de pendiente)");
    return pendiente(rutaRelativa, pie);
  }
  // Se escala al ancho util (o menos, si es una foto alta) manteniendo la proporcion.
  const buf = fs.readFileSync(ruta);
  const { tipo, w: wPx, h: hPx } = dimensiones(buf);
  const altoMaxPt = 420;
  let anchoPt = 468;                         // 6.5 pulgadas en puntos
  let altoPt = Math.round((anchoPt * hPx) / wPx);
  if (altoPt > altoMaxPt) {
    anchoPt = Math.round((anchoPt * altoMaxPt) / altoPt);
    altoPt = altoMaxPt;
  }

  return [
    new Paragraph({
      children: [new ImageRun({
        data: buf, type: tipo,
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

    // Inclusion de un fichero de codigo: <!-- incluir: ruta/relativa/a/docs -->
    // En GitHub es un comentario invisible; aqui se inserta el fichero entero,
    // de modo que el anexo del .docx nunca se desincroniza del codigo real.
    const inc = l.match(/^<!--\s*incluir:\s*(.+?)\s*-->$/);
    if (inc) {
      const texto = fs.readFileSync(path.join(RAIZ, "docs", inc[1]), "utf8");
      salida.push(bloqueCodigo(texto.replace(/\s+$/, "").split(/\r?\n/)));
      salida.push(new Paragraph({ text: "", spacing: { after: 140 } }));
      i++;
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
        // Informe corto: salto de pagina solo antes de los anexos.
        pageBreakBefore: nivel === 2 && /^Anexo/.test(h[2]),
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
  title: "Informe tecnico - Potenciometro con salida PWM (ESP32-C6)",
  description: "Practica de entrada analogica (ADC de 12 bits) y salida PWM para regular el brillo de un LED con el ESP32-C6.",
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
