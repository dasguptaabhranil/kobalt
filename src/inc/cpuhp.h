/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CPUHP_H
#define CPUHP_H

#include <stdint.h>

#define CPUHP_PARK_VECTOR   0x54

typedef enum {
    CPUHP_OFFLINE = 0,
    CPUHP_BRINGUP_PREPARE,
    CPUHP_BRINGUP_KICK,
    CPUHP_ONLINE,
    CPUHP_TEARDOWN,
} cpuhp_state_t;

typedef struct {
    void (*prepare)(unsigned cpu);
    void (*teardown)(unsigned cpu);
} cpuhp_notifier_t;

#define CPUHP_MAX_NOTIFIERS 8

void            cpuhp_init(void);
int             cpuhp_online(unsigned cpu);
int             cpuhp_offline(unsigned cpu);
cpuhp_state_t   cpuhp_get_state(unsigned cpu);
int             cpuhp_register_notifier(cpuhp_notifier_t *n);
void            cpuhp_unregister_notifier(cpuhp_notifier_t *n);
unsigned        cpuhp_online_count(void);

void            cpuhp_park_action(void);

#endif
