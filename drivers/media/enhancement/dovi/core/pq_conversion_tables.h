/* Fixed-point PQ tables. */
#ifndef DOVI_PQ_CONVERSION_TABLES_H
#define DOVI_PQ_CONVERSION_TABLES_H

/* One read-only owner in pq_conversion.c serves control and decoder users. */
extern const unsigned int pq_l2pqLutA[128];
extern const unsigned int pq_l2pqLutB[128];
extern const unsigned int pq_l2pqLutX[128];
extern const unsigned int pq_depq1024Lut[1024];

#endif
