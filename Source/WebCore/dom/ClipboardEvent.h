/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ClipboardEvent_h
#define ClipboardEvent_h

#include "Event.h"

namespace WebCore {

    class DataTransfer;

    struct ClipboardEventInit : public EventInit {
        ClipboardEventInit();
        RefPtr<DataTransfer> dataTransfer;
    };

    class ClipboardEvent final : public Event {
    public:
        virtual ~ClipboardEvent();

        static Ref<ClipboardEvent> create(const AtomicString& type, bool canBubbleArg, bool cancelableArg, DataTransfer* clipboardArg)
        {
            return adoptRef(*new ClipboardEvent(type, canBubbleArg, cancelableArg, clipboardArg));
        }

        static Ref<ClipboardEvent> createForBindings(const AtomicString& type, ClipboardEventInit& initializer)
        {
            return adoptRef(*new ClipboardEvent(type, initializer));
        }

        DataTransfer* internalDataTransfer() const override { return m_dataTransfer.get(); }

    private:
        ClipboardEvent(const AtomicString& type, bool canBubbleArg, bool cancelableArg, DataTransfer*);
        ClipboardEvent(const AtomicString& type, ClipboardEventInit&);

        EventInterface eventInterface() const override;
        bool isClipboardEvent() const override;

        RefPtr<DataTransfer> m_dataTransfer;
    };

} // namespace WebCore

#endif // ClipboardEvent_h
