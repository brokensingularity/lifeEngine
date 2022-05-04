/**
 * @file
 * @addtogroup WorldEd World editor
 *
 * Copyright Broken Singularity, All Rights Reserved.
 * Authors: Yehor Pohuliaka (zombiHello)
 */

#ifndef VIEWPORTWIDGET_H
#define VIEWPORTWIDGET_H

#include <QWidget>

#include "Render/Viewport.h"

/**
 * @ingroup WorldEd
 * Widget viewport for render scene and tick engine
 */
class WeViewportWidget : public QWidget
{
	Q_OBJECT

public:
	/**
	 * Constructor
	 * @param InParent					Parent widget
	 * @param InViewportClient			Viewport client
	 * @param InDeleteViewportClient	Is need delete viewport client in destroy this widget
	 */
	WeViewportWidget( QWidget* InParent = nullptr, FViewportClient* InViewportClient = nullptr, bool InDeleteViewportClient = false );

	/**
	 * Destructor
	 */
	virtual ~WeViewportWidget();

	/**
	 * Set enabled
	 * @param InIsEnabled		Is enabled viewport
	 */
	void SetEnabled( bool InIsEnabled );

	/**
	 * Set viewport type
	 * 
	 * @param InViewportClient			Viewport client
	 * @param InDeleteViewportClient	Is need delete viewport client in destroy this widget
	 */
	FORCEINLINE void SetViewportClient( FViewportClient* InViewportClient, bool InDeleteViewportClient )
	{
		viewportClient = InViewportClient;
		bDeleteViewportClient = InDeleteViewportClient;
		viewport.SetViewportClient( viewportClient );
	}

	/**
	 * Get engine viewport
	 * @return Return engine viewport
	 */
	FORCEINLINE const FViewport& GetViewport() const
	{
		return viewport;
	}

	/**
	 * Is enabled viewport
	 * @return Return TRUE if viewport is enabled, else return FALSE
	 */
	FORCEINLINE bool IsEnabled() const
	{
		return bEnabled;
	}

private:
	/**
	 * Init viewport
	 */
	void InitViewport();

	/**
	 * Event called when widget showed
	 * 
	 * @param[in] InEvent Event of show
	 */
	virtual void showEvent( QShowEvent* InEvent ) override;

	/**
	 * Return Qt paint engine
	 * @return Return Qt paint engine
	 */
	virtual QPaintEngine* paintEngine() const override;

	/**
	 * Event called when paint widget
	 * 
	 * @param[in] InEvent Event of paint widget
	 */
	virtual void paintEvent( QPaintEvent* InEvent ) override;

	/**
	 * Event called when resize widget
	 * 
	 * @param[in] InEvent Event of resize widget
	 */
	virtual void resizeEvent( QResizeEvent* InEvent ) override;

	/**
	 * Event called when wheel is moving
	 */
	virtual void wheelEvent( QWheelEvent* InEvent ) override;

	/**
	 * Event of mouse press
	 *
	 * @param InEvent Event of mouse press
	 */
	virtual void mousePressEvent( QMouseEvent* InEvent ) override;

	/**
	 * Event of mouse release
	 *
	 * @param InEvent Event of mouse release
	 */
	virtual void mouseReleaseEvent( QMouseEvent* InEvent ) override;

	/**
	 * Event of mouse move
	 *
	 * @param InEvent Event of mouse move
	 */
	virtual void mouseMoveEvent( QMouseEvent* InEvent ) override;

	/**
	 * Event of key press
	 *
	 * @param InEvent Event of key press
	 */
	virtual void keyPressEvent( QKeyEvent* InEvent ) override;

	/**
	 * Event of key release
	 *
	 * @param InEvent Event of key release
	 */
	virtual void keyReleaseEvent( QKeyEvent* InEvent ) override;

	bool					bEnabled;					/**< Is enabled viewport */
	bool					bInTick;					/**< Is added viewport to editor engine */
	bool					bDeleteViewportClient;		/**< Is need delete viewport client in destroy time */
	QPoint					mousePosition;				/**< Last mouse position */
	FViewport				viewport;					/**< Viewport of render */
	FViewportClient*		viewportClient;				/**< Viewport client */
};

#endif // !VIEWPORTWIDGET_H